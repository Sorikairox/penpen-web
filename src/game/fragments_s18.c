/* Pen Pen port, S18: the "functions" of port/symbols/functions.tsv that are pieces of another function (hw.h
 * HW_EXPORT_FRAGMENT): Ghidra split them off their parent at a literal pool, an epilogue reached by branches, a case
 * tail or the code after a call it took for one that does not return; or they are literal-pool data. Each parent is
 * hand-written C that does the piece's work; checked for every one: no call to it in port_work/s13/calls.txt other
 * than from its own parent (or a piece of it), no entry in port/symbols/entries.tsv, and no pointer to it in the
 * binary but in its parent's literal pool. Entering one on its own stops the run (hw.c: hw_fragment_entered). The
 * registers and frame slots each one inherits are in the S18 writers' reports (port_work/s18). */
#include "hw.h"

/* func_8C0163CE: the loading screen's second half (Ghidra split it at a literal pool) (parent func_8C01630E) */
HW_EXPORT_FRAGMENT(8c0163ce, 8c01630e)
/* func_8C02131C: the cast viewer update's last frame windows (motion set 8), in its frame (parent func_8C0200E8) */
HW_EXPORT_FRAGMENT(8c02131c, 8c0200e8)
/* func_8C02139E: the cast viewer update's shared epilogue (parent func_8C0200E8) */
HW_EXPORT_FRAGMENT(8c02139e, 8c0200e8)
/* func_8C021C24: the gallery draw's number block, in its frame (r14, the sprite) (parent func_8C021C10) */
HW_EXPORT_FRAGMENT(8c021c24, 8c021c10)
/* func_8C02206A: the config draw's entries loop, in its frame (parent func_8C022058) */
HW_EXPORT_FRAGMENT(8c02206a, 8c022058)
/* func_8C02280A: the constant-material setter's tail (fr4-fr7, its 0x10 frame) (parent func_8C0227FC) */
HW_EXPORT_FRAGMENT(8c02280a, 8c0227fc)
/* func_8C027628: the record screen draw's body (its frame) (parent func_8C027234) */
HW_EXPORT_FRAGMENT(8c027628, 8c027234)
/* func_8C027968: the record screen draw's final number block (MACL, r0, r5, @(0x50,r15)) (parent func_8C027234) */
HW_EXPORT_FRAGMENT(8c027968, 8c027234)
/* func_8C027D3C: a branch into the record screen draw's number block (parent func_8C027234) */
HW_EXPORT_FRAGMENT(8c027d3c, 8c027234)
/* func_8C028F16: the constant-material setter's tail (fr4-fr7, its 0x10 frame) (parent func_8C028F08) */
HW_EXPORT_FRAGMENT(8c028f16, 8c028f08)
/* func_8C029348: the text style reset's fall-through tail (r0, r5, fr4) (parent func_8C029332) */
HW_EXPORT_FRAGMENT(8c029348, 8c029332)
/* func_8C02AF7C: the race step's epilogue (reached by its branches) (parent func_8C029EFA) */
HW_EXPORT_FRAGMENT(8c02af7c, 8c029efa)
/* func_8C02BBB0: the three-player view draw's tail (its 0x40 frame) (parent func_8C02BAEE) */
HW_EXPORT_FRAGMENT(8c02bbb0, 8c02baee)
/* func_8C02BC24: the four-player view draw's tail (its 0x40 frame) (parent func_8C02BBCA) */
HW_EXPORT_FRAGMENT(8c02bc24, 8c02bbca)
/* func_8C02CC60: the panel draw's body (its frame slots @(0x20/0x24,r15), the sprite at r15) (parent func_8C02CC3E) */
HW_EXPORT_FRAGMENT(8c02cc60, 8c02cc3e)
/* func_8C0319F4: the racer update's second half (its @(0x20,r15)) (parent func_8C0317D4) */
HW_EXPORT_FRAGMENT(8c0319f4, 8c0317d4)
/* func_8C038A34: the material setter's tail (a jsr through the r3 it set) (parent func_8C038A16) */
HW_EXPORT_FRAGMENT(8c038a34, 8c038a16)
/* func_8C039484: the ribbon set update's body (r0, r9-r13, its frame) (parent func_8C03945C) */
HW_EXPORT_FRAGMENT(8c039484, 8c03945c)
/* func_8C0413FA: the course draw's body (fr0, its frame) (parent func_8C0413CC) */
HW_EXPORT_FRAGMENT(8c0413fa, 8c0413cc)
/* func_8C0439F8: the sky draw's case tail (its 0x10 frame) (parent func_8C04391E) */
HW_EXPORT_FRAGMENT(8c0439f8, 8c04391e)
/* func_8C04AC88: the constant-material setter's tail (fr4-fr7, its 0x10 frame) (parent func_8C04AC6A) */
HW_EXPORT_FRAGMENT(8c04ac88, 8c04ac6a)
/* func_8C04E036: the constant-material setter's tail (fr4-fr7, its 0x10 frame) (parent func_8C04E028) */
HW_EXPORT_FRAGMENT(8c04e036, 8c04e028)
/* func_8C050F48: type 0x19 step 0's tail (its 0x18 frame) (parent func_8C050BAE) */
HW_EXPORT_FRAGMENT(8c050f48, 8c050bae)
/* func_8C050F4C: type 0x19 step 0's tail (its 0x18 frame, r8, fr0) (parent func_8C050BAE) */
HW_EXPORT_FRAGMENT(8c050f4c, 8c050bae)
/* func_8C0510EE: type 0x19 step 1's tail (its 8-byte frame, r1, fr0) (parent func_8C05106C) */
HW_EXPORT_FRAGMENT(8c0510ee, 8c05106c)
/* func_8C0511CC: type 0x19 step 1's last block (parent func_8C05106C) */
HW_EXPORT_FRAGMENT(8c0511cc, 8c05106c)
/* func_8C052D00: literal-pool data (a float) after type 0x0B's draw: not code (parent func_8C052C70) */
HW_EXPORT_FRAGMENT(8c052d00, 8c052c70)
/* func_8C052D18: literal-pool data (floats) after type 0x0B's draw: not code (parent func_8C052C70) */
HW_EXPORT_FRAGMENT(8c052d18, 8c052c70)
/* func_8C052E0A: a piece of type 0x0B's contact response (its 0x34 frame) (parent func_8C052D20) */
HW_EXPORT_FRAGMENT(8c052e0a, 8c052d20)
/* func_8C052E16: a piece of type 0x0B's contact response (its 0x34 frame) (parent func_8C052D20) */
HW_EXPORT_FRAGMENT(8c052e16, 8c052d20)
/* func_8C052E62: a piece of type 0x0B's contact response (its 0x34 frame) (parent func_8C052D20) */
HW_EXPORT_FRAGMENT(8c052e62, 8c052d20)
/* func_8C0589C6: the constant-material setter's tail after the copy helper (fr4-fr7, its frame) (parent func_8C0589B8) */
HW_EXPORT_FRAGMENT(8c0589c6, 8c0589b8)
/* func_8C059CDC: the resting actor init's tail (r12-r14, r0, fr4-fr6) (parent func_8C059CAC) */
HW_EXPORT_FRAGMENT(8c059cdc, 8c059cac)
/* func_8C05A278: literal-pool data (the float 0.98) of type 0x27's init: not code (parent func_8C05A148) */
HW_EXPORT_FRAGMENT(8c05a278, 8c05a148)
/* func_8C05A614: type 0x27 part draw's tail (its 0x90 frame) (parent func_8C05A4CC) */
HW_EXPORT_FRAGMENT(8c05a614, 8c05a4cc)
/* func_8C05CD8C: the constant-material setter's tail after the copy helper (parent func_8C05CD7E) */
HW_EXPORT_FRAGMENT(8c05cd8c, 8c05cd7e)
/* func_8C05E184: the middle of type 0x30's node pose (r10-r14, r0, its 0x24 frame) (parent func_8C05E098) */
HW_EXPORT_FRAGMENT(8c05e184, 8c05e098)
/* func_8C05FF32: the constant-material setter's tail after the copy helper (parent func_8C05FF24) */
HW_EXPORT_FRAGMENT(8c05ff32, 8c05ff24)
/* func_8C061F1E: date-to-seconds' body (the month table copied into its 0x30 frame) (parent func_8C061F10) */
HW_EXPORT_FRAGMENT(8c061f1e, 8c061f10)
/* func_8C061FB8: seconds-to-date's body (the month table in its 0x30 frame) (parent func_8C061FA4) */
HW_EXPORT_FRAGMENT(8c061fb8, 8c061fa4)
/* func_8C062348: literal-pool data (a word 8c06224c loads): not code (parent func_8C06224C) */
HW_EXPORT_FRAGMENT(8c062348, 8c06224c)
/* func_8C023858: an unreferenced stub (no call, no pointer, no entry) that branches into the middle of the SetupMenu construction (parent func_8C022D80) */
HW_EXPORT_FRAGMENT(8c023858, 8c022d80)
