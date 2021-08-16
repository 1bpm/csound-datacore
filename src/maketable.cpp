#include <plugin.h>
#include "maketable.hpp"

int maketable(csnd::Csound* csound, int size, FUNC **tablep, int channels) {
    EVTBLK* evt;
    MYFLT* pf;

    evt = (EVTBLK*) csound->malloc(sizeof(EVTBLK));
    evt->opcod = 'f';
    evt->strarg = NULL;
    evt->pcnt = 5;
    pf = &evt->p[0];
    pf[0] = FL(0);
    pf[1] = FL(0);
    pf[2] = evt->p2orig = FL(0);
    pf[3] = evt->p3orig = -size;
    pf[4] = FL(2); // gen number
    pf[5] = FL(0);
    int n = csound->get_csound()->hfgens(csound->get_csound(), tablep, evt, 1);
    csound->free(evt);
    if (UNLIKELY(n != 0)) {
        return NOTOK;
    }
    
    FUNC* table = *tablep;
    
    table->soundend = size;
    table->nchanls = channels;
    table->flenfrms = size;
    table->gen01args.sample_rate = csound->sr();
    table->cpscvt = 0;
    table->cvtbas = LOFACT; // * csound->sr() * csound->get_csound()->onedsr;
    table->loopmode1 = 0;
    table->loopmode2 = 0;
    table->begin1 = 0;
    table->end1 = size;
    table->begin2 = 0;
    table->end2 = size;
    return OK;
}