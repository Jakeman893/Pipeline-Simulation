/***********************************************************************
 * File         : pipeline.cpp
 * Author       : Soham J. Desai 
 * Date         : 14th January 2014
 * Description  : Superscalar Pipeline for Lab2 ECE 6100
 **********************************************************************/

 #include "pipeline.h"
 #include <cstdlib>
 
 extern int32_t PIPE_WIDTH;
 extern int32_t ENABLE_MEM_FWD;
 extern int32_t ENABLE_EXE_FWD;
 extern int32_t BPRED_POLICY;
 
 /**********************************************************************
  * Support Function: Read 1 Trace Record From File and populate Fetch Op
  **********************************************************************/
 
 void pipe_get_fetch_op(Pipeline *p, Pipeline_Latch* fetch_op){
     uint8_t bytes_read = 0;
     bytes_read = fread(&fetch_op->tr_entry, 1, sizeof(Trace_Rec), p->tr_file);
 
     // check for end of trace
     if( bytes_read < sizeof(Trace_Rec)) {
       fetch_op->valid=false;
       p->halt_op_id=p->op_id_tracker;
       return;
     }
 
     // got an instruction ... hooray!
     fetch_op->valid=true;
     fetch_op->stall=false;
     fetch_op->is_mispred_cbr=false;
     p->op_id_tracker++;
     fetch_op->op_id=p->op_id_tracker;
     
     return; 
 }
 
 
 /**********************************************************************
  * Pipeline Class Member Functions 
  **********************************************************************/
 
 Pipeline * pipe_init(FILE *tr_file_in){
     printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);
 
     // Initialize Pipeline Internals
     Pipeline *p = (Pipeline *) calloc (1, sizeof (Pipeline));
 
     p->tr_file = tr_file_in;
     p->halt_op_id = ((uint64_t)-1) - 3;           
 
     // Allocated Branch Predictor
     if(BPRED_POLICY){
       p->b_pred = new BPRED(BPRED_POLICY);
     }
 
     return p;
 }
 
 
 /**********************************************************************
  * Print the pipeline state (useful for debugging)
  **********************************************************************/
 
 void pipe_print_state(Pipeline *p){
     std::cout << "--------------------------------------------" << std::endl;
     std::cout <<"cycle count : " << p->stat_num_cycle << " retired_instruction : " << p->stat_retired_inst << std::endl;
 
     uint8_t latch_type_i = 0;   // Iterates over Latch Types
     uint8_t width_i      = 0;   // Iterates over Pipeline Width
     for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
         switch(latch_type_i) {
             case 0:
                 printf(" FE: ");
                 break;
             case 1:
                 printf(" ID: ");
                 break;
             case 2:
                 printf(" EX: ");
                 break;
             case 3:
                 printf(" MEM: ");
                 break;
             default:
                 printf(" ---- ");
         }
     }
     printf("\n");
     for(width_i = 0; width_i < PIPE_WIDTH; width_i++) {
         for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
             if(p->pipe_latch[latch_type_i][width_i].valid == true) {
         printf(" %6u ",(uint32_t)( p->pipe_latch[latch_type_i][width_i].op_id));
             } else {
                 printf(" ------ ");
             }
         }
         printf("\n");
     }
     printf("\n");
 
 }
 
 
 /**********************************************************************
  * Pipeline Main Function: Every cycle, cycle the stage 
  **********************************************************************/
 
 void pipe_cycle(Pipeline *p)
 {
     p->stat_num_cycle++;
 
     pipe_cycle_WB(p);
     pipe_cycle_MEM(p);
     pipe_cycle_EX(p);
     pipe_cycle_ID(p);
     pipe_cycle_FE(p);
 
    //  pipe_print_state(p);
 }
 /**********************************************************************
  * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
  **********************************************************************/
 
void pipe_cycle_WB(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    Pipeline_Latch_Struct *stage = &p->pipe_latch[MEM_LATCH][ii];
    if(!stage->stall)
    {
      if(stage->valid){
        p->stat_retired_inst++;
        if(stage->op_id >= p->halt_op_id){
          p->halt=true;
        }
      }
    }
  }
}
 

 //--------------------------------------------------------------------//
 
 void pipe_cycle_MEM(Pipeline *p){
   int ii;
   for(ii=0; ii<PIPE_WIDTH; ii++){
     if(!p->pipe_latch[MEM_LATCH][ii].stall)
     {
       p->pipe_latch[MEM_LATCH][ii]=p->pipe_latch[EX_LATCH][ii];
     }
   }
 }
 
 //--------------------------------------------------------------------//
 
 void pipe_cycle_EX(Pipeline *p){
   int ii;
   for(ii=0; ii<PIPE_WIDTH; ii++){
     if(!p->pipe_latch[EX_LATCH][ii].stall)
     {
       p->pipe_latch[EX_LATCH][ii]=p->pipe_latch[ID_LATCH][ii];
     }
   }
 }
 
 //--------------------------------------------------------------------//
 
 void pipe_cycle_ID(Pipeline *p){
 int ii;
   for(ii=0; ii<PIPE_WIDTH; ii++){ 
     Pipeline_Latch *stage = &p->pipe_latch[ID_LATCH][ii];
 
     if(!stage->stall)
     {
       p->pipe_latch[ID_LATCH][ii]=p->pipe_latch[FE_LATCH][ii];
       p->pipe_latch[ID_LATCH][ii].stall = false;
 
       if(ENABLE_MEM_FWD){
         // TODO
       }
 
       if(ENABLE_EXE_FWD){
         // TODO
       }
 
     }
   }
 }
 
 //--------------------------------------------------------------------//
 
void pipe_cycle_FE(Pipeline *p){
  int ii;
  Pipeline_Latch fetch_op;
  bool tr_read_success;
  bool prev_stall = false;
  bool inter_depends = false;
  bool cc_write = false;
  int lane = 0;
  int dest_map[255] = {0};

  for(ii=0; ii<PIPE_WIDTH; ii++)
  {
    Pipeline_Latch *stage = &p->pipe_latch[FE_LATCH][ii];
    // if the previous instruction stalled, this one must as well and we don't need to check its dependencies
    if(prev_stall)
      stage->stall = prev_stall;
    else
    {
      stage->stall = false;
      // Check if source dependency for previous instructions in this stage
      if (stage->tr_entry.src1_needed || stage->tr_entry.src2_needed)
      {
        stage->stall |= (dest_map[stage->tr_entry.src1_reg] != 0);
        stage->stall |= (dest_map[stage->tr_entry.src1_reg] != 0);
      }

      // Increment value found in dest_map for the register written to
      if (stage->tr_entry.dest_needed)
        dest_map[stage->tr_entry.dest]++;

      if (cc_write && stage->tr_entry.cc_read)
        stage->stall = true;

      // Set cc_write to check if any instruction after this one should be stalled individually
      cc_write |= stage->tr_entry.cc_write;

      inter_depends |= stage->stall;

      // Check dependencies for each lane of the pipeline
      fe_dependence_check(stage, p->pipe_latch[EX_LATCH], p->pipe_latch[MEM_LATCH]);
    }

    // Based on stall value, set propagated instruction validity
    p->pipe_latch[ID_LATCH][ii].valid = (!stage->stall) && stage->valid;
    p->pipe_latch[ID_LATCH][ii].stall = false;

    // Informs next instruction that the previous instruction stalled, thus they must as well
    prev_stall = stage->stall;
  }

  // If there is a disconnect between instructions continuing down pipe and instructions stalled in a stage, reorder
  if(inter_depends)
  {
    int lane = 0;
    for(ii=0; ii< PIPE_WIDTH; ii++)
    {
      Pipeline_Latch *stage = &p->pipe_latch[FE_LATCH][ii];
      if(stage->stall)
      {
        // Order stalled instructions in increasing op_id order
        p->pipe_latch[FE_LATCH][lane++] = *stage;
        // Make sure to invalidate and de-stall original position
        stage->valid = false;
        stage->stall = false;
      }
    }
  }

  for(ii=0; ii<PIPE_WIDTH; ii++){
    Pipeline_Latch *stage = &p->pipe_latch[FE_LATCH][ii];
    //If not stalled
      //Fetch
        //Get new instruction
    if(!stage->stall)
    {
      //Fetch Instruction
      pipe_get_fetch_op(p, &fetch_op);
      
      //Branch prediction
      if(BPRED_POLICY)
        pipe_check_bpred(p, &fetch_op);

      //Copy op into FE LATCH
      p->pipe_latch[FE_LATCH][ii]=fetch_op;           
      
    }
  }
}
 
 
 //--------------------------------------------------------------------//
 
void pipe_check_bpred(Pipeline *p, Pipeline_Latch *fetch_op){
  // call branch predictor here, if mispred then mark in fetch_op
  // update the predictor instantly
  // stall fetch using the flag p->fetch_cbr_stall
}
 
 
 //--------------------------------------------------------------------//
 
void fe_dependence_check(Pipeline_Latch_Struct *festage, const Pipeline_Latch_Struct *exstage, const Pipeline_Latch_Struct *memstage)
{
  int ii;
  for(ii=0; ii < PIPE_WIDTH; ii++)
  {
    if(exstage[ii].tr_entry.dest_needed && exstage[ii].valid)
    {
      uint8_t dest = exstage[ii].tr_entry.dest;
      //Check sources against EX LATCH
      if(festage->tr_entry.src1_needed)
        festage->stall |= dest == festage->tr_entry.src1_reg;
      if(festage->tr_entry.src2_needed)
        festage->stall |= dest == festage->tr_entry.src2_reg;
    }
    if(memstage[ii].tr_entry.dest_needed && memstage[ii].valid)
    {
      //Check sources against MEM_LATCH
      uint8_t dest = memstage[ii].tr_entry.dest;
      //Check sources against EX LATCH
      if(festage->tr_entry.src1_needed)
        festage->stall |= dest == festage->tr_entry.src1_reg;
      if(festage->tr_entry.src2_needed)
        festage->stall |= dest == festage->tr_entry.src2_reg;
    }

    if(festage->tr_entry.cc_read)
      festage->stall |= ((memstage[ii].tr_entry.cc_write && memstage[ii].valid) | (exstage[ii].tr_entry.cc_write && exstage[ii].valid));
    
    festage->stall |= exstage[ii].stall;
  }
}