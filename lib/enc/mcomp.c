/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id$

 ********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include "codec_internal.h"

/* Initialises motion compentsation. */
void InitMotionCompensation ( CP_INSTANCE *cpi ){
  int i;
  int SearchSite=0;
  int Len;
  int LineStepY = cpi->stride[0];

  Len=((MAX_MV_EXTENT/2)+1)/2;

  /* How many search stages are there. */
  cpi->MVSearchSteps = 0;

  /* Set up offsets arrays used in half pixel correction. */
  cpi->HalfPixelRef2Offset[0] = -LineStepY - 1;
  cpi->HalfPixelRef2Offset[1] = -LineStepY;
  cpi->HalfPixelRef2Offset[2] = -LineStepY + 1;
  cpi->HalfPixelRef2Offset[3] = - 1;
  cpi->HalfPixelRef2Offset[4] = 0;
  cpi->HalfPixelRef2Offset[5] = 1;
  cpi->HalfPixelRef2Offset[6] = LineStepY - 1;
  cpi->HalfPixelRef2Offset[7] = LineStepY;
  cpi->HalfPixelRef2Offset[8] = LineStepY + 1;

  cpi->HalfPixelXOffset[0] = -1;
  cpi->HalfPixelXOffset[1] = 0;
  cpi->HalfPixelXOffset[2] = 1;
  cpi->HalfPixelXOffset[3] = -1;
  cpi->HalfPixelXOffset[4] = 0;
  cpi->HalfPixelXOffset[5] = 1;
  cpi->HalfPixelXOffset[6] = -1;
  cpi->HalfPixelXOffset[7] = 0;
  cpi->HalfPixelXOffset[8] = 1;

  cpi->HalfPixelYOffset[0] = -1;
  cpi->HalfPixelYOffset[1] = -1;
  cpi->HalfPixelYOffset[2] = -1;
  cpi->HalfPixelYOffset[3] = 0;
  cpi->HalfPixelYOffset[4] = 0;
  cpi->HalfPixelYOffset[5] = 0;
  cpi->HalfPixelYOffset[6] = 1;
  cpi->HalfPixelYOffset[7] = 1;
  cpi->HalfPixelYOffset[8] = 1;


  /* Generate offsets for 8 search sites per step. */
  while ( Len>0 ) {
    /* Another step. */
    cpi->MVSearchSteps += 1;

    /* Compute offsets for search sites. */
    cpi->MVOffsetX[SearchSite] = -Len;
    cpi->MVOffsetY[SearchSite++] = -Len;
    cpi->MVOffsetX[SearchSite] = 0;
    cpi->MVOffsetY[SearchSite++] = -Len;
    cpi->MVOffsetX[SearchSite] = Len;
    cpi->MVOffsetY[SearchSite++] = -Len;
    cpi->MVOffsetX[SearchSite] = -Len;
    cpi->MVOffsetY[SearchSite++] = 0;
    cpi->MVOffsetX[SearchSite] = Len;
    cpi->MVOffsetY[SearchSite++] = 0;
    cpi->MVOffsetX[SearchSite] = -Len;
    cpi->MVOffsetY[SearchSite++] = Len;
    cpi->MVOffsetX[SearchSite] = 0;
    cpi->MVOffsetY[SearchSite++] = Len;
    cpi->MVOffsetX[SearchSite] = Len;
    cpi->MVOffsetY[SearchSite++] = Len;

    /* Contract. */
    Len /= 2;
  }

  /* Compute pixel index offsets. */
  for ( i=SearchSite-1; i>=0; i-- )
    cpi->MVPixelOffsetY[i] = (cpi->MVOffsetY[i]*LineStepY) + cpi->MVOffsetX[i];
}

static ogg_uint32_t GetInterErr (CP_INSTANCE *cpi, 
				 unsigned char * NewDataPtr,
				 unsigned char * RefDataPtr1,
				 unsigned char * RefDataPtr2 ) {
  ogg_int32_t   DiffVal;
  ogg_int32_t   RefOffset = (int)(RefDataPtr1 - RefDataPtr2);

  /* Mode of interpolation chosen based upon on the offset of the
     second reference pointer */
  if ( RefOffset == 0 ) {
    DiffVal = dsp_inter8x8_err (cpi->dsp, NewDataPtr,
				RefDataPtr1, cpi->stride[0]);
  }else{
    DiffVal = dsp_inter8x8_err_xy2 (cpi->dsp, NewDataPtr, 
				    RefDataPtr1, RefDataPtr2, 
				    cpi->stride[0]);
  }

  /* Compute and return population variance as mis-match metric. */
  return DiffVal;
}

static ogg_uint32_t GetHalfPixelSumAbsDiffs (CP_INSTANCE *cpi,
					     unsigned char * SrcData,
					     unsigned char * RefDataPtr1,
					     unsigned char * RefDataPtr2,
					     ogg_uint32_t ErrorSoFar,
					     ogg_uint32_t BestSoFar ) {
  
  ogg_uint32_t  DiffVal = ErrorSoFar;
  ogg_int32_t   RefOffset = (int)(RefDataPtr1 - RefDataPtr2);
  
  if ( RefOffset == 0 ) {
    /* Simple case as for non 0.5 pixel */
    DiffVal += dsp_sad8x8 (cpi->dsp, SrcData,
			   RefDataPtr1, cpi->stride[0]);
  } else  {
    DiffVal += dsp_sad8x8_xy2_thres (cpi->dsp, SrcData,
				     RefDataPtr1, 
				     RefDataPtr2, 
				     cpi->stride[0],
				     BestSoFar);
  }

  return DiffVal;
}

ogg_uint32_t GetMBIntraError (CP_INSTANCE *cpi, 
			      macroblock_t *mp){

  ogg_uint32_t  IntraError = 0;
  dsp_save_fpu (cpi->dsp);
  unsigned char *cp = cpi->frag_coded;
  ogg_uint32_t *bp = cpi->frag_buffer_index;
  
  /* Add together the intra errors for those blocks in the macro block
     that are coded (Y only) */
  if ( cp[mp->y[0]] )
    IntraError += dsp_intra8x8_err (cpi->dsp, &cpi->frame[bp[mp->y[0]]],cpi->stride[0]);
  if ( cp[mp->y[1]] )
    IntraError += dsp_intra8x8_err (cpi->dsp, &cpi->frame[bp[mp->y[1]]],cpi->stride[0]);
  if ( cp[mp->y[2]] )
    IntraError += dsp_intra8x8_err (cpi->dsp, &cpi->frame[bp[mp->y[2]]],cpi->stride[0]);
  if ( cp[mp->y[3]] )
    IntraError += dsp_intra8x8_err (cpi->dsp, &cpi->frame[bp[mp->y[3]]],cpi->stride[0]);

  dsp_restore_fpu (cpi->dsp);
  return IntraError;
}

ogg_uint32_t GetMBInterError (CP_INSTANCE *cpi,
                              unsigned char * SrcPtr,
                              unsigned char * RefPtr,
			      macroblock_t *mp,
                              ogg_int32_t LastXMV,
                              ogg_int32_t LastYMV){
  ogg_uint32_t  PixelsPerLine = cpi->stride[0];
  ogg_int32_t   RefPixelOffset = ((LastYMV/2) * PixelsPerLine) + (LastXMV/2);
  ogg_int32_t   RefPtr2Offset = 0;
  
  ogg_uint32_t  InterError = 0;
  
  unsigned char * SrcPtr1;
  unsigned char * RefPtr1;
  unsigned char *cp = cpi->frag_coded;
  ogg_uint32_t *bp = cpi->frag_buffer_index;

  dsp_save_fpu (cpi->dsp);
  
  /* Work out the second reference pointer offset. */
  if ( LastXMV % 2 ) {
    if ( LastXMV > 0 )
      RefPtr2Offset += 1;
    else
      RefPtr2Offset -= 1;
  }
  if ( LastYMV % 2 ) {
    if ( LastYMV > 0 )
      RefPtr2Offset += PixelsPerLine;
    else
      RefPtr2Offset -= PixelsPerLine;
  }

  /* Add together the errors for those blocks in the macro block that
     are coded (Y only) */
  if ( cp[mp->y[0]] ) {
    SrcPtr1 = &SrcPtr[bp[mp->y[0]]];
    RefPtr1 = &RefPtr[bp[mp->y[0]] + RefPixelOffset];
    InterError += GetInterErr(cpi, SrcPtr1, RefPtr1, &RefPtr1[RefPtr2Offset] );
  }

  if ( cp[mp->y[1]] ) {
    SrcPtr1 = &SrcPtr[bp[mp->y[1]]];
    RefPtr1 = &RefPtr[bp[mp->y[1]] + RefPixelOffset];
    InterError += GetInterErr(cpi, SrcPtr1, RefPtr1, &RefPtr1[RefPtr2Offset] );
    
  }
  
  if ( cp[mp->y[2]] ) {
    SrcPtr1 = &SrcPtr[bp[mp->y[2]]];
    RefPtr1 = &RefPtr[bp[mp->y[2]] + RefPixelOffset];
    InterError += GetInterErr(cpi, SrcPtr1, RefPtr1, &RefPtr1[RefPtr2Offset] );
  }

  if ( cp[mp->y[3]] ) {
    SrcPtr1 = &SrcPtr[bp[mp->y[3]]];
    RefPtr1 = &RefPtr[bp[mp->y[3]] + RefPixelOffset];
    InterError += GetInterErr(cpi, SrcPtr1, RefPtr1, &RefPtr1[RefPtr2Offset] );
  }
  
  dsp_restore_fpu (cpi->dsp);
  
  return InterError;
}

ogg_uint32_t GetMBMVInterError (CP_INSTANCE *cpi,
                                unsigned char *RefFramePtr,
				macroblock_t *mp,
                                ogg_int32_t *MVPixelOffset,
                                mv_t *MV ) {
  ogg_uint32_t  Error = 0;
  ogg_uint32_t  MinError;
  ogg_uint32_t  InterMVError = 0;

  ogg_int32_t   i;
  ogg_int32_t   x=0, y=0;
  ogg_int32_t   step;
  ogg_int32_t   SearchSite=0;

  unsigned char *SrcPtr[4] = {NULL, NULL, NULL, NULL};
  unsigned char *RefPtr[4] = {NULL, NULL, NULL, NULL};
  int            BestBlockOffset=0;
  int            disp[4];
  int            off = 0;

  /* Half pixel variables */
  ogg_int32_t   HalfPixelError;
  ogg_int32_t   BestHalfPixelError;
  unsigned char   BestHalfOffset;
  unsigned char * RefDataPtr1;
  unsigned char * RefDataPtr2;
  unsigned char *cp = cpi->frag_coded;
  ogg_uint32_t *bp = cpi->frag_buffer_index;

  dsp_save_fpu (cpi->dsp);

  /* Note which of the four blocks in the macro block are to be
     included in the search. */
  disp[0] = cp[mp->y[0]];
  disp[1] = cp[mp->y[1]];
  disp[2] = cp[mp->y[2]];
  disp[3] = cp[mp->y[3]];

  if(disp[0]){
    SrcPtr[0] = &cpi->frame[bp[mp->y[0]]];
    RefPtr[0] = &RefFramePtr[bp[mp->y[0]]];
    Error += dsp_sad8x8 (cpi->dsp, SrcPtr[0], RefPtr[0],
                         cpi->stride[0]);
  }
  if(disp[1]){
    SrcPtr[1] = &cpi->frame[bp[mp->y[1]]];
    RefPtr[1] = &RefFramePtr[bp[mp->y[1]]];
    Error += dsp_sad8x8 (cpi->dsp, SrcPtr[1], RefPtr[1],
                         cpi->stride[0]);
  }
  if(disp[2]){
    SrcPtr[2] = &cpi->frame[bp[mp->y[2]]];
    RefPtr[2] = &RefFramePtr[bp[mp->y[2]]];
    Error += dsp_sad8x8 (cpi->dsp, SrcPtr[2], RefPtr[2],
                         cpi->stride[0]);
  }
  if(disp[3]){
    SrcPtr[3] = &cpi->frame[bp[mp->y[3]]];
    RefPtr[3] = &RefFramePtr[bp[mp->y[3]]];
    Error += dsp_sad8x8 (cpi->dsp, SrcPtr[3], RefPtr[3],
                         cpi->stride[0]);
  }

  /* Set starting values to results of 0, 0 vector. */
  MinError = Error;
  BestBlockOffset = 0;
  x = 0;
  y = 0;
  MV->x = 0;
  MV->y = 0;

  /* Proceed through N-steps. */
  for (  step=0; step<cpi->MVSearchSteps; step++ ) {
    /* Search the 8-neighbours at distance pertinent to current step.*/
    for ( i=0; i<8; i++ ) {
      /* Set pointer to next candidate matching block. */
      int loff = off + MVPixelOffset[SearchSite];
      Error = 0;
      
      /* Get the score for the current offset */
      if ( disp[0] ) 
        Error += dsp_sad8x8 (cpi->dsp, SrcPtr[0], RefPtr[0] + loff,
                             cpi->stride[0]);
      
      if ( disp[1] && (Error < MinError) ) 
        Error += dsp_sad8x8_thres (cpi->dsp, SrcPtr[1], RefPtr[1] + loff,
				   cpi->stride[0], MinError);
      
      if ( disp[2] && (Error < MinError) ) 
        Error += dsp_sad8x8_thres (cpi->dsp, SrcPtr[2], RefPtr[2] + loff,
				   cpi->stride[0],  MinError);
      
      if ( disp[3] && (Error < MinError) ) 
        Error += dsp_sad8x8_thres (cpi->dsp, SrcPtr[3], RefPtr[3] + loff,
				   cpi->stride[0],  MinError);

      if ( Error < MinError ) {
        /* Remember best match. */
        MinError = Error;
        BestBlockOffset = loff;
	
	/* Where is it. */
        x = MV->x + cpi->MVOffsetX[SearchSite];
        y = MV->y + cpi->MVOffsetY[SearchSite];
      }

      /* Move to next search location. */
      SearchSite += 1;
    }

    /* Move to best location this step. */
    off = BestBlockOffset;
    MV->x = x;
    MV->y = y;
  }

  /* Factor vectors to 1/2 pixel resoultion. */
  MV->x = (MV->x * 2);
  MV->y = (MV->y * 2);

  /* Now do the half pixel pass */
  BestHalfOffset = 4;     /* Default to the no offset case. */
  BestHalfPixelError = MinError;

  /* Get the half pixel error for each half pixel offset */
  for ( i=0; i < 9; i++ ) {
    HalfPixelError = 0;

    if ( disp[0] ) {
      RefDataPtr1 = RefPtr[0] + BestBlockOffset;
      RefDataPtr2 = RefDataPtr1 + cpi->HalfPixelRef2Offset[i];
      HalfPixelError =
        GetHalfPixelSumAbsDiffs(cpi, SrcPtr[0], RefDataPtr1, RefDataPtr2,
                         HalfPixelError, BestHalfPixelError );
    }

    if ( disp[1]  && (HalfPixelError < BestHalfPixelError) ) {
      RefDataPtr1 = RefPtr[1] + BestBlockOffset;
      RefDataPtr2 = RefDataPtr1 + cpi->HalfPixelRef2Offset[i];
      HalfPixelError =
        GetHalfPixelSumAbsDiffs(cpi, SrcPtr[1], RefDataPtr1, RefDataPtr2,
                         HalfPixelError, BestHalfPixelError );
    }

    if ( disp[2] && (HalfPixelError < BestHalfPixelError) ) {
      RefDataPtr1 = RefPtr[2] + BestBlockOffset;
      RefDataPtr2 = RefDataPtr1 + cpi->HalfPixelRef2Offset[i];
      HalfPixelError =
        GetHalfPixelSumAbsDiffs(cpi, SrcPtr[2], RefDataPtr1, RefDataPtr2,
                         HalfPixelError, BestHalfPixelError );
    }

    if ( disp[3] && (HalfPixelError < BestHalfPixelError) ) { 
      RefDataPtr1 = RefPtr[3] + BestBlockOffset;
      RefDataPtr2 = RefDataPtr1 + cpi->HalfPixelRef2Offset[i];
      HalfPixelError =
        GetHalfPixelSumAbsDiffs(cpi, SrcPtr[3], RefDataPtr1, RefDataPtr2,
                         HalfPixelError, BestHalfPixelError );
    }

    if ( HalfPixelError < BestHalfPixelError ) {
      BestHalfOffset = (unsigned char)i;
      BestHalfPixelError = HalfPixelError;
    }
  }

  /* Half pixel adjust the MV */
  MV->x += cpi->HalfPixelXOffset[BestHalfOffset];
  MV->y += cpi->HalfPixelYOffset[BestHalfOffset];

  /* Get the error score for the chosen 1/2 pixel offset as a variance. */
  InterMVError = GetMBInterError( cpi, cpi->frame, RefFramePtr, mp,
                                  MV->x, MV->y);
  
  dsp_restore_fpu (cpi->dsp);
  
  /* Return score of best matching block. */
  return InterMVError;
}

ogg_uint32_t GetMBMVExhaustiveSearch (CP_INSTANCE *cpi,
                                      unsigned char *RefFramePtr,
				      macroblock_t *mp,
                                      mv_t *MV ) {
  ogg_uint32_t  Error = 0;
  ogg_uint32_t  MinError = HUGE_ERROR;
  ogg_uint32_t  InterMVError = 0;

  ogg_int32_t   i, j;
  ogg_int32_t   x=0, y=0;

  unsigned char *SrcPtr[4] = {NULL,NULL,NULL,NULL};
  unsigned char *RefPtr[4] = {NULL,NULL,NULL,NULL};
  int            BestBlockOffset=0;

  int            disp[4];

  /* Half pixel variables */
  ogg_int32_t   HalfPixelError;
  ogg_int32_t   BestHalfPixelError;
  unsigned char   BestHalfOffset;
  unsigned char * RefDataPtr1;
  unsigned char * RefDataPtr2;
  int off;
  unsigned char *cp = cpi->frag_coded;
  ogg_uint32_t *bp = cpi->frag_buffer_index;

  dsp_save_fpu (cpi->dsp);

  /* Note which of the four blocks in the macro block are to be
     included in the search. */
  disp[0] = cp[mp->y[0]];
  disp[1] = cp[mp->y[1]];
  disp[2] = cp[mp->y[2]];
  disp[3] = cp[mp->y[3]];

  if(disp[0]){
    SrcPtr[0] = &cpi->frame[bp[mp->y[0]]];
    RefPtr[0] = &RefFramePtr[bp[mp->y[0]]];
  }
  if(disp[1]){
    SrcPtr[1] = &cpi->frame[bp[mp->y[1]]];
    RefPtr[1] = &RefFramePtr[bp[mp->y[1]]];
  }
  if(disp[2]){
    SrcPtr[2] = &cpi->frame[bp[mp->y[2]]];
    RefPtr[2] = &RefFramePtr[bp[mp->y[2]]];
  }
  if(disp[3]){
    SrcPtr[3] = &cpi->frame[bp[mp->y[3]]];
    RefPtr[3] = &RefFramePtr[bp[mp->y[3]]];
  }

  off = - ((MAX_MV_EXTENT/2) * cpi->stride[0]) - (MAX_MV_EXTENT/2);

  /* Search each pixel alligned site */
  for ( i = 0; i < (ogg_int32_t)MAX_MV_EXTENT; i ++ ) {
    /* Starting position in row */
    int loff = off;
    
    for ( j = 0; j < (ogg_int32_t)MAX_MV_EXTENT; j++ ) {
      Error = 0;

      /* Summ errors for each block. */
      if ( disp[0] ) 
        Error += dsp_sad8x8 (cpi->dsp, SrcPtr[0], RefPtr[0]+loff, cpi->stride[0]);
      if ( disp[1] )
        Error += dsp_sad8x8 (cpi->dsp, SrcPtr[1], RefPtr[1]+loff, cpi->stride[0]);
      if ( disp[2] )
        Error += dsp_sad8x8 (cpi->dsp, SrcPtr[2], RefPtr[2]+loff, cpi->stride[0]);
      if ( disp[3] )
        Error += dsp_sad8x8 (cpi->dsp, SrcPtr[3], RefPtr[3]+loff, cpi->stride[0]);
      
      /* Was this the best so far */
      if ( Error < MinError ) {
        MinError = Error;
        BestBlockOffset = loff;
        x = 16 + j - MAX_MV_EXTENT;
        y = 16 + i - MAX_MV_EXTENT;
      }
      
      /* Move the the next site */
      loff ++;
    }

    /* Move on to the next row. */
    off += cpi->stride[0];

  }

  /* Factor vectors to 1/2 pixel resoultion. */
  MV->x = (x * 2);
  MV->y = (y * 2);

  /* Now do the half pixel pass */
  BestHalfOffset = 4;     /* Default to the no offset case. */
  BestHalfPixelError = MinError;

  /* Get the half pixel error for each half pixel offset */
  for ( i=0; i < 9; i++ ) {
    HalfPixelError = 0;

    if ( disp[0] ) {
      RefDataPtr1 = RefPtr[0]+BestBlockOffset;
      RefDataPtr2 = RefDataPtr1 + cpi->HalfPixelRef2Offset[i];
      HalfPixelError =
        GetHalfPixelSumAbsDiffs(cpi, SrcPtr[0], RefDataPtr1, RefDataPtr2,
				HalfPixelError, BestHalfPixelError );
    }

    if ( disp[1]  && (HalfPixelError < BestHalfPixelError) ) {
      RefDataPtr1 = RefPtr[1]+BestBlockOffset;
      RefDataPtr2 = RefDataPtr1 + cpi->HalfPixelRef2Offset[i];
      HalfPixelError =
        GetHalfPixelSumAbsDiffs(cpi, SrcPtr[1], RefDataPtr1, RefDataPtr2,
				HalfPixelError, BestHalfPixelError );
    }
    
    if ( disp[2] && (HalfPixelError < BestHalfPixelError) ) {
      RefDataPtr1 = RefPtr[2]+BestBlockOffset;
      RefDataPtr2 = RefDataPtr1 + cpi->HalfPixelRef2Offset[i];
      HalfPixelError =
        GetHalfPixelSumAbsDiffs(cpi, SrcPtr[2], RefDataPtr1, RefDataPtr2,
				HalfPixelError, BestHalfPixelError );
    }
    
    if ( disp[3] && (HalfPixelError < BestHalfPixelError) ) {
      RefDataPtr1 = RefPtr[3]+BestBlockOffset;
      RefDataPtr2 = RefDataPtr1 + cpi->HalfPixelRef2Offset[i];
      HalfPixelError =
        GetHalfPixelSumAbsDiffs(cpi, SrcPtr[3], RefDataPtr1, RefDataPtr2,
				HalfPixelError, BestHalfPixelError );
    }

    if ( HalfPixelError < BestHalfPixelError ){
      BestHalfOffset = (unsigned char)i;
      BestHalfPixelError = HalfPixelError;
    }
  }

  /* Half pixel adjust the MV */
  MV->x += cpi->HalfPixelXOffset[BestHalfOffset];
  MV->y += cpi->HalfPixelYOffset[BestHalfOffset];

  /* Get the error score for the chosen 1/2 pixel offset as a variance. */
  InterMVError = GetMBInterError( cpi, cpi->frame, RefFramePtr, mp,
                                  MV->x, MV->y );

  dsp_restore_fpu (cpi->dsp);

  /* Return score of best matching block. */
  return InterMVError;
}

static ogg_uint32_t GetBMVExhaustiveSearch (CP_INSTANCE *cpi,
                                            unsigned char *RefFramePtr,
					    int fi,
                                            mv_t *MV ) {
  ogg_uint32_t  Error = 0;
  ogg_uint32_t  MinError = HUGE_ERROR;
  ogg_uint32_t  InterMVError = 0;

  ogg_int32_t   i, j;
  ogg_int32_t   x=0, y=0;

  unsigned char *SrcPtr = NULL;
  unsigned char *RefPtr;
  unsigned char *CandidateBlockPtr=NULL;
  unsigned char *BestBlockPtr=NULL;

  /* Half pixel variables */
  ogg_int32_t   HalfPixelError;
  ogg_int32_t   BestHalfPixelError;
  unsigned char   BestHalfOffset;
  unsigned char * RefDataPtr2;
  int  bi = cpi->frag_buffer_index[fi];

  /* Set up the source pointer for the block. */
  SrcPtr = &cpi->frame[bi];
  RefPtr = &RefFramePtr[bi];
  RefPtr = RefPtr - ((MAX_MV_EXTENT/2) * cpi->stride[0]) - (MAX_MV_EXTENT/2);
  
  /* Search each pixel alligned site */
  for ( i = 0; i < (ogg_int32_t)MAX_MV_EXTENT; i ++ ) {
    /* Starting position in row */
    CandidateBlockPtr = RefPtr;
    
    for ( j = 0; j < (ogg_int32_t)MAX_MV_EXTENT; j++ ){
      /* Get the block error score. */
      Error = dsp_sad8x8 (cpi->dsp, SrcPtr, CandidateBlockPtr, cpi->stride[0] );

      /* Was this the best so far */
      if ( Error < MinError ) {
        MinError = Error;
        BestBlockPtr = CandidateBlockPtr;
        x = 16 + j - MAX_MV_EXTENT;
        y = 16 + i - MAX_MV_EXTENT;
      }

      /* Move the the next site */
      CandidateBlockPtr ++;
    }

    /* Move on to the next row. */
    RefPtr += cpi->stride[0];
  }

  /* Factor vectors to 1/2 pixel resoultion. */
  MV->x = (x * 2);
  MV->y = (y * 2);

  /* Now do the half pixel pass */
  BestHalfOffset = 4;     /* Default to the no offset case. */
  BestHalfPixelError = MinError;

  /* Get the half pixel error for each half pixel offset */
  for ( i=0; i < 9; i++ ) {
    RefDataPtr2 = BestBlockPtr + cpi->HalfPixelRef2Offset[i];
    HalfPixelError =
      GetHalfPixelSumAbsDiffs(cpi, SrcPtr, BestBlockPtr, RefDataPtr2,
			      0, BestHalfPixelError );
    
    if ( HalfPixelError < BestHalfPixelError ){
      BestHalfOffset = (unsigned char)i;
      BestHalfPixelError = HalfPixelError;
    }
  }

  /* Half pixel adjust the MV */
  MV->x += cpi->HalfPixelXOffset[BestHalfOffset];
  MV->y += cpi->HalfPixelYOffset[BestHalfOffset];

  /* Get the variance score at the chosen offset */
  RefDataPtr2 = BestBlockPtr + cpi->HalfPixelRef2Offset[BestHalfOffset];

  InterMVError =
    GetInterErr(cpi, SrcPtr, BestBlockPtr, RefDataPtr2 );

  /* Return score of best matching block. */
  return InterMVError;
}

ogg_uint32_t GetFOURMVExhaustiveSearch (CP_INSTANCE *cpi,
                                        unsigned char * RefFramePtr,
					macroblock_t *mp,
                                        mv_t *MV ) {
  ogg_uint32_t  InterMVError;
  unsigned char *cp = cpi->frag_coded;

  dsp_save_fpu (cpi->dsp);

  /* For the moment the 4MV mode is only deemed to be valid 
     if all four Y blocks are to be updated */
  /* This may be adapted later. */
  if ( cp[mp->y[0]] &&
       cp[mp->y[1]] &&
       cp[mp->y[2]] &&
       cp[mp->y[3]] ) {
    
    /* Reset the error score. */
    InterMVError = 0;
    
    /* Get the error component from each coded block */
    InterMVError +=
      GetBMVExhaustiveSearch(cpi, RefFramePtr, mp->y[0], &(MV[0]) );
    InterMVError +=
      GetBMVExhaustiveSearch(cpi, RefFramePtr, mp->y[1], &(MV[1]) );
    InterMVError +=
      GetBMVExhaustiveSearch(cpi, RefFramePtr, mp->y[2], &(MV[2]) );
    InterMVError +=
      GetBMVExhaustiveSearch(cpi, RefFramePtr, mp->y[3], &(MV[3]) );
  }else{
    InterMVError = HUGE_ERROR;
  }
  
  dsp_restore_fpu (cpi->dsp);

  /* Return score of best matching block. */
  return InterMVError;
}



