TFILON FOR MRE (Nokia 225 and other MediaTek MRE phones)
========================================================
Unofficial port of "Tfilon" 0.2.21 (a J2ME Hebrew prayer book by Uri Hollander, tfilon.com) to the MediaTek MRE
platform. Written in C from the decompiled logic of the original MIDlet; all prayer texts and the Hebrew bitmap
font are taken from the original jar.

INSTALL
 Nokia Series 30+ phones only start an MRE app that carries the IMSI of the SIM inside the phone (stored in the file
 as "9" + the 15 digits). Two offline ways to create your personal file - you only need the 15-digit IMSI:
   * open Tfilon_signer.html in any browser, type the IMSI, press the button -> downloads Tfilon.vxp
   * or: python3 dist/patch_imsi.py Tfilon.vxp <IMSI>        -> writes Tfilon_signed.vxp
 Then copy the file to the phone / memory card (the MRE folder is the usual place) and open it from the file manager.
 The file only runs with that SIM; with another SIM create a new file. If the phone refuses to start it because of
 memory, lower the request (signer page: Advanced; script: --ram 768).
 tools/make_signer_html.py rebuilds Tfilon_signer.html from any .vxp.

KEYS
 Up/Down (or 2/8)   scroll / move          Left/Right (or 4/6)  page down/up in the reader
 OK (or 5)          select                 Left softkey         select / section menu in the reader
 Right softkey/Back go back / exit         #                    full-screen toggle in the reader

WHAT IS PORTED (checked against the original code)
 * Hebrew calendar + all holiday/day-type flags: identical to the original for every day 1990-2070 (29,585 days)
 * sunrise/sunset for all 120 cities: identical to the original (max difference 0 s); Hebrew day switches at sunset
 * prayer-text interpreter (nusach a/s/em, conditions, includes, omer / Sukkot / hoshanot generators, bookmarks,
   colours), word wrap, bitmap Hebrew renderer with the original vowel/dagesh placement rules, settings and cities
 * "Daily times" screen is reduced: date, sunrise, sunset and the same status tags (no full zmanim table, no Daf Yomi)
 * not ported: random quotes, "names for the sick" text entry, parasha/Torah-reading display, status ticker

BUILD FROM SOURCE (Linux)
 1. apt install gcc-arm-none-eabi libnewlib-arm-none-eabi binutils-arm-none-eabi python3
 2. git clone https://github.com/gtrxAC/mre-makefile ; cd mre-makefile ; rm src/test.c
 3. copy this package's src/* into mre-makefile/src/ and manifest.json over the existing one
 4. in the Makefile set  APP_FILE_NAME = Tfilon  and change -march=armv5te to -march=armv4t
 5. make OBJCOPY=arm-none-eabi-objcopy      (output: Tfilon.vxp)
 tools/gen_data.py regenerates src/data_gen.c from the original Tfilon.jar. test/ holds the host-side regression
 tests (calendar / sunrise vs. the original Java classes, document stress test, ARM emulator harness).

LICENCE / CREDIT
 Tfilon is (c) Uri Hollander. The original About screen says it may be distributed freely as long as no fee is charged.
 This port keeps that About screen. Use it for yourself; ask the author before sharing it further.
