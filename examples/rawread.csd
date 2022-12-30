<CsoundSynthesizer>
<CsOptions>
-odac
</CsOptions>
<CsInstruments>
sr = 44100
kr = 4410
nchnls = 2
0dbfs = 1
seed 0


instr 1
	a1 rawread "/usr/bin/zip"
    a1 *= 0.1
    outs a1, a1
endin

instr 2
    aL, aR rawread "/usr/bin/zip", 1
    outs aL*0.1, aR*0.1
endin

instr 3
    ifn rawreadtable "/usr/bin/zip"
    a1 loscil 0.1, 1, ifn, 1
    
    outs a1, a1
endin

</CsInstruments>
<CsScore>
i3 0 60
</CsScore>
</CsoundSynthesizer>
