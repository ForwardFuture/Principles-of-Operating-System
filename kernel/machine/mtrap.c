#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() { panic("Illegal instruction!"); }

static void handle_misaligned_load() { panic("Misaligned Load!"); }

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

// added @lab1_3
static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

//
// point out errorline
//
static void errorline() {

  sprint("Runtime error at ");

  uint64 fault_code_segment = read_csr(mepc);
  uint64 fault_source_code_line = 0;
  uint64 fault_file = 0;

  for(int i = 0;; i++) {
    if(fault_code_segment == current->line[i].addr) {
      fault_source_code_line = current->line[i].line;
      fault_file = current->line[i].file;
      break;
    }
  }

  char path[1000];
  int path_len = 0;
  for(int i = 0;; i++) {
    if((current->dir)[current->file[fault_file].dir][i] == '\0')break;
    path[path_len++] = (current->dir)[current->file[fault_file].dir][i];
  }
  path[path_len++] = '/';
  for(int i = 0;; i++) {
    if(current->file[fault_file].file[i] == '\0')break;
    path[path_len++] = current->file[fault_file].file[i];
  }
  path[path_len] = '\0';

  sprint("%s:%lld\n", path, fault_source_code_line);

  char buf[5000];
  int siz = 5000;
  spike_file_t* fp = spike_file_open(path, O_RDONLY, 0);
  spike_file_read(fp, buf, siz);
  spike_file_close(fp);
  
  for(int i = 0; i < siz; i++) {
    if(fault_source_code_line == 1) {
      if(buf[i] == '\n') {
        sprint("\n");
        break;
      }
      else sprint("%c", buf[i]);
    }
    else if(buf[i] == '\n') {
      fault_source_code_line--;
    }
  }
  return;
}


//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_MTIMER:
      errorline();
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      errorline();
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      errorline();
      handle_load_access_fault();
      break;
    case CAUSE_STORE_ACCESS:
      errorline();
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      errorline();
      handle_illegal_instruction();
      break;
    case CAUSE_MISALIGNED_LOAD:
      errorline();
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      errorline();
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}
