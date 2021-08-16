<CsoundSynthesizer>
<CsOptions>
-o/usr/tmp/tst.wav
</CsOptions>
<CsInstruments>
sr = 44100
kr = 441
nchnls = 2
0dbfs = 1
seed 0


instr 1
	idata[] memps
	ipid = idata[int(random(0, lenarray(idata)-1))]
	kr1 line 0, p3, 1
	kr2 line 0, p3, 1
	;a1 memson2 ipid, 0.4, 0.5; kr2
	a1 memson ipid, 0;kr1
	a1 dcblock a1*0.1
	anull init 0
	if (p4 == 0) then
		outs a1, anull
	else
		outs anull, a1
	endif

endin

instr scd
	itime = 0
	ilen = 10
	while (itime < p3) do
		index = 0
		while (index < 3) do
			event_i "i", 1, itime, ilen, 0
			event_i "i", 1, itime, ilen, 1
			index += 1
		od
		itime += ilen
	od
endin

</CsInstruments>
<CsScore>
i"scd" 0 60
</CsScore>
</CsoundSynthesizer>
