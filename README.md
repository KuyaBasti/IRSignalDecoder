# IR Signal Decoder

A CC3200 that turns a TV remote into a phone keypad. A GPIO interrupt watches the output of an IR demodulator on **PIN_58** and times every low pulse against a free-running **SysTick** — a start pulse longer than 2 ms opens a frame, then twelve pulse-widths are shifted MSB-first into a **12-bit button code** (`0x7EF` is the "2/ABC" key, `0xD6F` is MUTE). The main loop feeds accepted codes into a **multi-tap texting engine** — press 4 twice for "h", old-phone style — draws the message live on a **128×128 SSD1351 OLED** over SPI, and on MUTE ships it as a `'$'`-terminated byte stream over **UART1** to a second CC3200 running the same firmware, which prints it on *its* screen. Two boards, two remotes, SMS from 2003.

The interesting part isn't the OLED plumbing — it's how little state the whole thing runs on. The ISR is nothing but a **shift register with a stopwatch**: one `data` int, one edge counter, one flag. Every decision that feels like it needs a timer callback — "is this a repeat frame?", "same key again, or a new letter?" — is made *lazily in the main loop* by comparing against a 40 ms-granularity wall clock that the same SysTick maintains as a side job. Even the multi-tap commit is lazy: the letter you're cycling through is drawn on screen immediately but only lands in the message buffer **when the next keypress proves you're done with it**.

![Wiring diagram](docs/wiring-diagram.svg)

---

## Table of Contents

1. [How a Keypress Becomes a Character](#how-a-keypress-becomes-a-character)
2. [Repository Map](#repository-map)
3. [The Wiring](#the-wiring)
4. [The IR Decoder — a Shift Register with a Stopwatch](#the-ir-decoder--a-shift-register-with-a-stopwatch)
5. [The Multi-Tap Engine](#the-multi-tap-engine)
6. [The Display Stack](#the-display-stack)
7. [The UART1 Link](#the-uart1-link)
8. [Build & Flash](#build--flash)
9. [Known Limitations & Sharp Edges](#known-limitations--sharp-edges)
10. [Provenance & Acknowledgments](#provenance--acknowledgments)

---

## How a Keypress Becomes a Character

```mermaid
flowchart TD
    IR["IR demodulator on PIN_58<br/>both-edge GPIO interrupt"] --> EDGE{"falling or<br/>rising edge?"}
    EDGE -- "falling — burst starts" --> RST["reset SysTick current count<br/>(first edge also zeroes data + edge_counter)"]
    EDGE -- "rising — burst ends" --> DELTA["delta = low-pulse width in µs"]
    DELTA --> WHICH{"which edge<br/>of the frame?"}
    WHICH -- "edge 0, delta ≤ 2000 µs" --> ABORT["not a start bit — abort frame"]
    WHICH -- "edge 0, delta > 2000 µs" --> OPEN["frame open"]
    WHICH -- "edges 1–12" --> BIT["data &lt;&lt;= 1<br/>delta ≤ 1000 µs → data |= 1"]
    BIT --> FULL{"13 edges<br/>seen?"}
    FULL -- "yes" --> FLAG["pin_out_intflag = 1 —<br/>12-bit code ready for the loop"]
    FLAG --> GATE{"main loop:<br/>≥ 200 ms since last code?"}
    GATE -- "no — repeat frame" --> DROP["ignored"]
    GATE -- "yes" --> COMMIT{"different button, or same<br/>button after 1500 ms?"}
    COMMIT -- "yes" --> APPEND["commit pending letter to msg[],<br/>x += 6, cycle = 0"]
    COMMIT -- "no — still cycling" --> CYCLE["cycle++ — next letter<br/>of the same key"]
    APPEND --> SW["switch on the code:<br/>letter · space · caps · delete · send"]
    CYCLE --> SW
    SW -- "letter keys 2–9" --> DRAW["drawChar at (x, 64)<br/>white on black, live preview"]
    SW -- "MUTE 0xD6F" --> SEND["append '$' — UART1Send(msg)<br/>+ Report to the console"]
```

The same loop also polls UART1: when bytes from the peer board arrive, `UART1Receive` collects up to 20 characters (or until `'$'`), and the message is drawn along the top row of the OLED — received text on top, your own composition mid-screen.

## Repository Map

```text
IRSignalDecoder/
├── README.md               # you are here
├── SYSTEM-DESIGN.md        # the architecture-level view
├── docs/
│   └── wiring-diagram.svg  # the schematic above
├── main.c                  # all the project-authored logic: IR ISR, SysTick clocks,
│                           #   multi-tap engine, UART1 link, main loop
├── pin_mux_config.c/.h     # TI PinMux-generated pin routing (IR input, GSPI, both UARTs,
│                           #   OLED CS/DC/RESET GPIOs)
├── Adafruit_OLED.c         # SSD1351 driver — SPI byte writes + init sequence
│                           #   (the lab's TODO 1–3 stubs, implemented here)
├── Adafruit_SSD1351.h      # SSD1351 command set, 128×128 geometry
├── Adafruit_GFX.c/.h       # Adafruit graphics core (C port): drawChar, lines, rects
├── glcdfont.h              # 5×7 ASCII bitmap font
├── uart_if.c               # TI SDK console helpers (Report/Message) for UART0
├── cc3200v1p32.cmd         # TI linker script — RAM-resident image at 0x20004000
├── .project / .ccsproject / .cproject   # CCS project "lab3-pt4", cloned from the
│                           #   SDK's uart_demo projectspec
├── targetConfigs/          # CC3200 debug-probe configuration (CC3200.ccxml)
├── .launches/              # CCS debug launch config
├── Debug/                  # build artifacts, checked in (lab3-pt4.bin/.out/.map)
├── README.html             # TI's original uart_demo readme — an SDK leftover
└── FILELIST.txt            # generated file listing (tooling artifact)
```

## The Wiring

Everything below is read straight out of [pin_mux_config.c](pin_mux_config.c) and [main.c](main.c) — see the [schematic](docs/wiring-diagram.svg):

| Signal | CC3200 pin | Peripheral / GPIO | Goes to |
|---|---|---|---|
| IR data | **PIN_58** | GPIO03 (GPIOA0 mask `0x8`), both-edge interrupt | IR demodulator OUT |
| OLED data | **PIN_07** | GSPI MOSI | SSD1351 `SI` |
| OLED clock | **PIN_05** | GSPI CLK | SSD1351 `CL` |
| OLED chip select | **PIN_18** | GPIO28 (GPIOA3 mask `0x10`), output | SSD1351 `OC` |
| OLED data/command | **PIN_45** | GPIO31 (GPIOA3 mask `0x80`), output | SSD1351 `DC` |
| OLED reset | **PIN_08** | GPIO17 (GPIOA2 mask `0x2`), output | SSD1351 `R` |
| Board-to-board TX | **PIN_01** | UART1 TX, 115200 8N1 | peer's PIN_02 |
| Board-to-board RX | **PIN_02** | UART1 RX | peer's PIN_01 |
| Debug console | **PIN_55 / PIN_57** | UART0 TX/RX | LaunchPad USB backchannel |
| — | PIN_50, PIN_06 | GSPI CS / MISO | configured, unused (see sharp edges) |

The IR receiver is a demodulating module (the kind sold for 38 kHz TV-remote work) whose output idles high and pulls low for each IR burst — that polarity is baked into the ISR, which treats the falling edge as burst-start. The remote used here produces **12-bit codes**, captured by hand and recorded in a comment block at the top of `main.c` ("Binary & HEX from manual decoding").

## The IR Decoder — a Shift Register with a Stopwatch

`GPIOA0IntHandler` in [main.c](main.c) fires on **both edges** of PIN_58. SysTick is configured with a reload of 3,200,000 ticks — exactly **40 ms at the CC3200's fixed 80 MHz** — and the ISR treats it as a resettable stopwatch: every falling edge writes `NVIC_ST_CURRENT` (any write clears the counter), and the matching rising edge computes the low-pulse width in microseconds via the `TICKS_TO_US` macro.

The framing rules, in full:

- **Edge 0 is the start bit** — a low pulse that must exceed **2000 µs**, or the frame is aborted on the spot.
- **Edges 1–12 are data** — `data <<= 1`, and a pulse **≤ 1000 µs** shifts in a `1` (long pulse = `0`). This is pulse-*width* coding, not NEC's pulse-distance coding.
- **At the 13th edge** the frame is complete: `pin_out_intflag = 1` and the ISR goes quiet. The main loop picks the 12-bit code out of `data` at its leisure.

The same SysTick moonlights as a wall clock: its wrap handler adds 40 to `global_time` (milliseconds), which the main loop uses for the **200 ms repeat-suppression gate** (remotes retransmit while a button is held) and the **1500 ms multi-tap timeout**.

Button map, verified against the `switch` in `main.c`:

| Code | Button | Action | Code | Button | Action |
|---|---|---|---|---|---|
| `0x6EF` | 0 | space | `0x5EF` | 6 | m n o |
| `0xFEF` | 1 | caps-lock toggle | `0x9EF` | 7 | p q r s |
| `0x7EF` | 2 | a b c | `0x1EF` | 8 | t u v |
| `0xBEF` | 3 | d e f | `0xEEF` | 9 | w x y z |
| `0x3EF` | 4 | g h i | `0x22F` | LAST | delete |
| `0xDEF` | 5 | j k l | `0xD6F` | MUTE | send |

## The Multi-Tap Engine

The clever bit is that **the pending letter is committed by the *next* keypress, not by a timer**. Pressing a key draws its current letter at `(x, 64)` immediately — cycling a key just redraws the same cell — but `msg[]` only grows when a *different* button arrives (or the same one after 1500 ms), at which point the pending letter is appended, `x` advances one 6-pixel cell, and the cycle counter resets. Pressing MUTE is itself "a different button", so it commits the last letter before appending the `'$'` terminator and transmitting — nothing is ever lost to a missing timeout handler.

Three buttons are special-cased out of the commit rule (the condition excludes `prev_data` ∈ {caps, send, delete}), and caps-lock even carries a small `curr_cycle--` compensation so that toggling case mid-word doesn't eat a tap. Case itself is one subtraction: `curr_letter = 'a' + cycle - (caps_lock * 32)`.

Delete (`LAST`) commits the pending letter, removes the last character of `msg`, steps `x` back a cell, and draws a space over the dead glyph — a visual backspace with no framebuffer, because the SSD1351's RAM *is* the framebuffer.

## The Display Stack

Three layers, top to bottom:

- **[Adafruit_GFX.c](Adafruit_GFX.c)** — the classic Adafruit graphics core ported from C++ to C. The app really only uses `drawChar` (5×7 glyphs from [glcdfont.h](glcdfont.h) in a 6×8 cell), `fillRect`, and `fillScreen`.
- **[Adafruit_OLED.c](Adafruit_OLED.c)** — the SSD1351 driver. `writeCommand`/`writeData` wrap every single byte in a GPIO chip-select assert (PIN_18 low), a hardware `SPICSEnable`, one `SPIDataPut`, a dummy `SPIDataGet` to drain the RX FIFO, and the reverse. `Adafruit_Init` runs the panel bring-up sequence (command lock, clock div, remap `0x74`, contrast, VSL…) after a GPIO reset pulse on PIN_08.
- **GSPI** — configured in `main.c` at **100 kHz, mode 0, 8-bit words, software-controlled active-high CS**.

Screen real estate is two one-line mailboxes: **row `y=0`** shows the last message received from the peer, **row `y=64`** shows the message being composed, both white-on-black (`0xFFFF` on `0x0000`). `fillRect(0, y, 128, 9, 0x0000)` is the eraser.

## The UART1 Link

The inter-board protocol is as thin as it can be: `InitUART1` sets **UARTA1 to 115200 8N1** (FIFO enabled), `UART1Send` busy-waits `UARTBusy` and puts one byte at a time, and the message framing is a single sentinel — **`'$'` marks end of message**. `UART1Receive` spins collecting bytes until it sees `'$'` or hits **20 characters**, then null-terminates. There are no ACKs, no checksums, no device IDs — the wire carries exactly the text you typed, and both ends run the same binary, so the link is symmetric by construction. After sending, an `UtilsDelay(80000)` (~3 ms at 80 MHz) lets the line drain before the loop moves on.

The UART0 console (TI's [uart_if.c](uart_if.c), 115200 8N1 over the LaunchPad's USB backchannel) narrates both directions: `Message to be sent: …` and `Received: …`.

## Build & Flash

Honest requirements — this is a Code Composer Studio project for real hardware; there is no simulator, no host build, and no test suite:

- **Hardware**: a TI **CC3200 LaunchPad** (two for messaging), an Adafruit **SSD1351 128×128 OLED**, a demodulating **IR receiver module**, and a TV remote that emits the 12-bit codes above (or re-capture your own and edit the `switch`).
- **Code Composer Studio** — the project was built with **CCS 7.3** and TI ARM compiler **16.9.4.LTS** (per [.cproject](.cproject)); newer CCS versions can import it.
- **CC3200 SDK 1.5.0** — the project references it via the `CC3200_SDK_ROOT` path variable (originally `C:/ti/CC3200SDK_1.5.0/cc3200-sdk`). Two build inputs live *only* in the SDK: `startup_ccs.c` (a linked resource in [.project](.project)) and `uart_if.h`, plus `driverlib.a`.

Steps:

1. Install the CC3200 SDK and fix the `CC3200_SDK_ROOT` / `CC3200_EXAMPLE_ROOT` path variables in `.project` (they point at a `C:\ti` install).
2. **File → Import → CCS Projects**, select this directory; the project imports as `lab3-pt4`.
3. Build (`Ctrl+B`) and debug via the bundled [targetConfigs/CC3200.ccxml](targetConfigs/CC3200.ccxml). The linker script places the whole image in **RAM at 0x20004000**, so a debug launch runs it directly — power-cycling loses it unless you flash the `.bin` with UniFlash.
4. Open a serial terminal on the LaunchPad's COM port at **115200 8N1** to watch the console. A prebuilt image is checked in at `Debug/lab3-pt4.bin`.

## Known Limitations & Sharp Edges

Honest notes — some are scope cuts, some are latent bugs the demo never hits:

- **Receiving blocks everything.** Once the first byte arrives, `UART1Receive` spins until it has seen `'$'` or 20 bytes. A peer that dies mid-message (or line noise that eats the `'$'`) freezes the UI forever — the IR ISR still decodes, but the loop never returns to look. A 20-byte burst without a terminator also leaves the eventual `'$'` in the FIFO, which the next poll reads as an empty message.
- **`msg[50]` has no bounds check.** The commit path appends without limits; type past ~48 characters and it overflows into whatever global the linker placed next. You'd never notice from the screen, because the display gives out first: `x` advances 6 px per character and `drawChar` clips at column 128, so everything after the ~21st character is composed blind — but still transmitted.
- **A truncated IR frame desynchronizes the decoder.** If a transmission dies before 13 edges, `reading_data` stays `true` and the *next* frame's start bit is swallowed as a data bit — codes come out garbled until edge counts realign. The comments describe a SysTick-based "transmission ended" reset (`systick_cnt`), but nothing in the decode path ever reads it; the timeout is half-wired.
- **Multi-tap resumes mid-cycle after a delete.** Send and caps reset/compensate the cycle counter; delete doesn't. The first tap of a letter key right after LAST yields the *second* letter of its group ('b' where you expect 'a').
- **Holding a button types the alphabet.** Repeat suppression is a single 200 ms window on a clock with 40 ms resolution — hold a key and the remote's repeat frames land as fresh presses, cycling the letter under your thumb.
- **The chip-select is doubled and half of it goes nowhere.** Every byte toggles both the hardware GSPI CS (PIN_50, configured active-high, software-controlled) and the GPIO CS on PIN_18 — but only PIN_18 is wired to the OLED. PIN_50 and PIN_06 (MISO) are muxed and dangling.
- **The driver's own comment disagrees with the wiring.** `Adafruit_OLED.c` says RESET is "wired to GPIO28, pin 18" — in this code GPIO28/PIN_18 is chip select, and RESET is GPIO17/PIN_08.
- **100 kHz SPI is leisurely.** A full-screen fill is 32,768 data bytes, each wrapped in its own CS dance and dummy FIFO read — the boot-time `fillScreen` visibly crawls. Single-row updates are why the UI stays usable.
- **Inherited init quirks.** `Adafruit_Init` sends the clock-div (`0xF1`), precharge (`0x32`), and VCOMH (`0x05`) parameters via `writeCommand` instead of `writeData` — faithfully reproducing the historical Adafruit Arduino library it was ported from.
- **The repo does not build standalone.** `startup_ccs.c` and `uart_if.h` are not in the tree — they come from the CC3200 SDK via linked resources and include paths. No SDK, no build.
- **Small leftovers** — `msg` is `volatile` but passed to `strlen`/`Report` (qualifier silently dropped), a global `l` is shadowed by a local `int l` in the commit branch, and an earlier README described NEC 32-bit decoding, packet structs with checksums, message history, and auto-detection of remotes; none of that exists in this code. The decoder is 12-bit pulse-width, and the protocol is a dollar sign.

## Provenance & Acknowledgments

This is a university embedded-systems lab project — the CCS project is named **`lab3-pt4`**, and the checked-in build rules reference a workspace under an **"EEC 172"** directory. The project was cloned from the CC3200 SDK's **`uart_demo`** example (per the `.ccsproject` origin and the leftover TI `README.html`), and built up from there. Judged by file headers and content:

- **Project-authored** — the IR decode ISR, SysTick timing, multi-tap engine, UART1 link, and main loop in [main.c](main.c) (the 12-bit button codes were captured by hand, per the comment block), plus the SPI `writeCommand`/`writeData`/`Adafruit_Init` bodies filled in at the lab's `TODO 1–3` markers in [Adafruit_OLED.c](Adafruit_OLED.c).
- **[Adafruit Industries](https://www.adafruit.com)** — the graphics core ([Adafruit_GFX.c](Adafruit_GFX.c)), SSD1351 command set, and 5×7 font, written by Limor Fried/Ladyada, BSD-licensed, ported from Arduino C++ to C for this platform.
- **Texas Instruments** — [pin_mux_config.c](pin_mux_config.c) (generated by TI PinMux 4.0.1543), [uart_if.c](uart_if.c), the linker script, and the SDK scaffolding, all BSD-licensed.

No teammates are credited in the repo or its git history; the only other personal identifier is the Windows username `dalmamun` in the checked-in `Debug/` build paths, whose relationship to the committer is not recorded.

See [SYSTEM-DESIGN.md](SYSTEM-DESIGN.md) for the architecture-level view: the full data-flow diagram, the ideas behind the design, and the numbers that matter.
