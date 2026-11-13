#!/bin/sh
# S18 (tools/bisect_cup4b.sh LIST): with the exports in LIST translated again (renamed HW_OFF_BISECT[_R], restored after,
# the translation regenerated), does cup4b's hand run to frame 1770 match the pure one (port_work/s18/pure1770.frames, made
# by the pure run of that route)? prints "same" or the first differing frame lines. Never run it while another job builds.
# 1770 match the pure one? prints "same" or "diff at <first differing frame>"
set -u
cd ~/Code/penpen
export PATH=/Applications/Docker.app/Contents/Resources/bin:$PATH
for a in $(cat "$1"); do
  f=$(grep -rl "^HW_EXPORT[A-Z_]*($a," src/ | head -1); [ -n "$f" ] || continue
  sed -i '' "s|^HW_EXPORT($a,|HW_OFF_BISECT($a,|; s|^HW_EXPORT_RESUMABLE($a,|HW_OFF_BISECT_R($a,|" "$f"
done
HAND=1 GEN=genhand tools/s3.sh gen > /dev/null 2>&1
P="8C1B7E54=FFFFFFFF@1100,8C1B7E58=FFFFFFFF@1100,8C1B7E5C=FFFFFFFF@1100,8C1B7E60=FFFFFFFF@1100"
GEN=genhand tools/s3.sh native --frames 1770 --pad 600:START,700:A,1100:START,1200:A,1500:A,1620:RIGHT,1640:RIGHT,1660:RIGHT,1700:A,1730:DOWN,1760:A --poke "$P" --verbose 0 --frame-log 1 --digest 1 --digest-nostack 1 > /tmp/claude-501/hand1770.log 2>&1
grep -a '^frame ' /tmp/claude-501/hand1770.log | sed 's/ interp [0-9]* / interp - /' > /tmp/claude-501/hand1770.frames
grep -aE 'stopped|crash|^hw |^rt (stop|crash)' /tmp/claude-501/hand1770.log | grep -v 'frame limit' | head -3
for f in $(grep -rlE "^HW_OFF_BISECT(_R)?\(" src/); do sed -i '' 's|^HW_OFF_BISECT(|HW_EXPORT(|; s|^HW_OFF_BISECT_R(|HW_EXPORT_RESUMABLE(|' "$f"; done
left=$(grep -rlE "^HW_OFF_BISECT(_R)?\(" src/ | wc -l | tr -d ' ')
d=$(diff port_work/s18/pure1770.frames /tmp/claude-501/hand1770.frames | head -1)
echo "$(wc -l < "$1" | tr -d ' ') off: ${d:-same} (markers left $left)"

# restore the translation the build compiles against
HAND=1 GEN=genhand tools/s3.sh gen > /dev/null 2>&1
