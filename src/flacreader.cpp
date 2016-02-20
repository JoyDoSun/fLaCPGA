/**********************************
 * FlacReader class               *
 **********************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "frames.hpp"
#include "subframes.hpp"
#include "metadata.hpp"
#include "constants.hpp"

#include "bitreader.hpp"
#include "flacreader.hpp"


FLACReader::FLACReader(FILE *f){
    _fr = new FileReader(f);
    
    _meta = new FLACMetaData();
    _frame = new FLACFrameHeader();
    _subframe = new FLACSubFrameHeader();
    _c = new FLACSubFrameConstant();
    _v = new FLACSubFrameVerbatim();\
    _f = new FLACSubFrameFixed();
    _l = new FLACSubFrameLPC();
    
}


int FLACReader::read(int32_t **pcm_buf){
    read_meta();
    
    int numChannels = _meta->getStreamInfo()->getNumChannels();
    int totalInterSamples = _meta->getStreamInfo()->getTotalSamples() * numChannels;
    int samplesRead = 0;
    
    (*pcm_buf) = (int32_t *)malloc(sizeof(int32_t)*totalInterSamples);
    
    while (samplesRead < totalInterSamples){
        samplesRead += read_frame((*pcm_buf) + samplesRead);
    }
    
    fprintf(stderr, "Wanted: %d  -- diff : %d\n", totalInterSamples, totalInterSamples - samplesRead);
    
    return samplesRead;
}

int FLACReader::read_meta(){
    _meta->read(_fr);
    fprintf(stderr, "--METADATA\n");
    _meta->print(stderr);
    
    _meta->getStreamInfo()->print(stderr);
    return 1;
}

int FLACReader::read_frame(int32_t *data){
    _frame->reconstruct();
    _frame->read(_fr);
    //fprintf(stderr, "--FRAME HEADER\n");
    //_frame->print(stderr);
    
    int ch;
    int samplesRead = 0;
    
    for (ch = 0; ch < _frame->getNumChannels(); ch++){
        _subframe->reconstruct();
        _subframe->read(_fr);
        fprintf(stderr, "----SUBFRAME HEADER\n");
        _subframe->print(stdout);
        
        uint8_t chanType = _frame->getChannelType();
        uint8_t bps = _frame->getSampleSize();
        uint16_t blockSize = _frame->getBlockSize();
        
        switch (chanType){
            case CH_MID: //Mid side
            case CH_LEFT: //Left Side
                if (ch == 1) bps++;
                break;
            case CH_RIGHT: // Right side
                if (ch == 0) bps++;
                break;
        }
        
        switch (_subframe->getSubFrameType()){
            case SUB_CONSTANT:            
                _c->reconstruct(bps, blockSize);
                samplesRead += _c->read(_fr, data);
                break;
            case SUB_VERBATIM:
                _v->reconstruct(bps, blockSize);
                samplesRead += _v->read(_fr, data);
                break;
            case SUB_FIXED:
                _f->reconstruct(bps, blockSize, _subframe->getFixedOrder());
                samplesRead += _f->read(_fr, data);
                break;
            case SUB_LPC:
                _l->reconstruct(bps, blockSize, _subframe->getLPCOrder());
                samplesRead += _l->read(_fr, data);
                break;
            case SUB_INVALID:
                fprintf(stderr, "Invalid subframe type\n");
                _fr->read_error();
        }
        
        
        /*for (int i = 0; i < blockSize; i++){
            printf("%d\n", data[i]);
        }*/
    }
    
    _frame->read_padding(_fr);
    _frame->read_footer(_fr);
    
    return samplesRead;
}