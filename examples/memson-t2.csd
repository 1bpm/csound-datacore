<CsoundSynthesizer>
<CsOptions>
-o/usr/tmp/tst.wav
</CsOptions>
<CsInstruments>
sr = 48000
kr = 4800
nchnls = 2
0dbfs = 1
seed 0


instr 1
	idata[] memps
	ipid = idata[int(random(0, lenarray(idata)-1))]
	print ipid
	kr1 line 0, p3, 1
	kr2 line 0, p3, 1
	;a1 memson2 ipid, 0.4, 0.5; kr2
	a1 memson ipid, 0;kr1
	a1 dcblock a1*0.1
	outs a1, a1
endin


</CsInstruments>
<CsScore>
i1 0 60
</CsScore>
</CsoundSynthesizer>
