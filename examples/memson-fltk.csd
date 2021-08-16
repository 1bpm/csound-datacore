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

FLpanel "memread", 640, 640
    gkx, gky, ihx, ihy FLjoy "Memory", 0, 1, 0, 1, 0, 0, -1, -1, 400, 400, 10, 10
    gilabel FLbox "No PID", 1, 11, 14, 600, 50, 10, 450
    gkpv1, ibt FLbutton "PVX", 1, 0, 2, 50, 20, 450, 10, -1
FLpanelEnd
FLrun

instr 1
    idata[] memps
    ipid = idata[int(random(0, lenarray(idata)-1))]
    Sname = sprintf("%s (%d)", mempsname(ipid), ipid)
    FLsetText Sname, gilabel
    a1 memson ipid, gkx, gky, 44100, 1
    a1 dcblock a1
    if (gkpv1 == 1) then 
        ir = 512
        kstens = abs(oscil(0.7, 5)) + 1.5
        f1 pvsanal a1, ir, ir/4, ir, 1
        f2 pvstencil f1, 0, kstens, 1
        a1 pvsynth f2
    endif
    a1 *= 0.1
    outs a1, a1
endin


</CsInstruments>
<CsScore>
f1 0 512 -43 "noise.pvx" 0 

i1 0 3600
</CsScore>
</CsoundSynthesizer>
