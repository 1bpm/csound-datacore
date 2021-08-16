<CsoundSynthesizer>
<CsOptions>
-odac
</CsOptions>
<CsInstruments>
sr = 44100
kr = 441
nchnls = 2
0dbfs = 1
seed 0


instr 1
        ips[] memps
        ipid = ips[int(random(0, lenarray(ips)-1))]
	ifn mem2tab ipid, 1

        a1 loscil 1, 1, ifn, 1
	a1 dcblock a1*0.1

        outs a1, a1
endin

</CsInstruments>
<CsScore>
i1 0 60
</CsScore>
</CsoundSynthesizer>
