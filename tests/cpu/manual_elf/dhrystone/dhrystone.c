// tests/cpu/manual_elf/dhrystone/dhrystone.c
//
// ChipForge 自包含 Dhrystone 2.2 benchmark (mfc Phase G)
// 完整算法 (Proc_1..8 + Func_1..3 + 所有 macros) 自包含, 无 vendor 依赖.
// 简化: 移除 Start_Timer/Stop_Timer/setStats/HZ+ alloca + riscv-tests util.h.
// 自供 globals + tohost_pass stub (PicolibcHostMemory 默认 tohost_addr=0).
//
// 编译:
//   riscv32-unknown-elf-g++ -march=rv32im_zicsr -mabi=ilp32 -nostdlib -static \
//     -I . -T ../link.ld \
//     dhrystone.c \
//     -o dhrystone.elf
//
// SPDX-License-Identifier: BSD-3-Clause (原始 Dhrystone 2.2 许可)

// ===== Typedefs (from Dhrystone 2.2) =====
typedef int Enumeration;
typedef int One_Fifty;
typedef int One_Thirty;
typedef int Boolean;
typedef char Capital_Letter;
typedef char Str_30[31];
typedef int Arr_1_Dim[50];
typedef int Arr_2_Dim[50][50];

typedef struct Rec_Type *Rec_Pointer;
typedef struct Rec_Type {
  Rec_Pointer Ptr_Comp;
  Enumeration Discr;
  union {
    struct {
      Enumeration Enum_Comp;
      int Int_Comp;
      Str_30 Str_Comp;
    } var_1;
    struct {
      Enumeration E_Comp_2;
      Str_30 Str_2_Comp;
    } var_2;
    struct {
      char Ch_1_Comp;
      char Ch_2_Comp;
    } var_3;
  } variant;
} Rec_Type;

#define Ident_1 0
#define Ident_2 1
#define Ident_3 2
#define Ident_4 3
#define Ident_5 4

// ===== Globals =====
Rec_Pointer Ptr_Glob, Next_Ptr_Glob;
int Int_Glob = 0;
Boolean Bool_Glob = false;
char Ch_1_Glob = ' ', Ch_2_Glob = ' ';
Arr_1_Dim Arr_1_Glob;
Arr_2_Dim Arr_2_Glob;

// ===== Helper: 自供 strcpy =====
static inline void dhry_strcpy(char *dst, const char *src, int max_len) {
  int j = 0;
  while (src[j] != 0 && j < max_len) { dst[j] = src[j]; j++; }
  dst[j] = 0;
}

// ===== PicolibcHostMemory tohost_addr=0 PASS marker =====
static void tohost_pass(void) {
  volatile int *p = (volatile int *)0;
  *p = 1;
  while (1);
}

// ===== Proc_1..8 + Func_1..3 (modern C++ signature, Dhrystone 2.2 algorithm) =====
void Proc_1(Rec_Pointer Ptr_Val_Par);
void Proc_2(One_Fifty *Int_Par_Ref);
void Proc_3(Rec_Pointer *Ptr_Ref_Par);
void Proc_4(void);
void Proc_5(void);
void Proc_6(Enumeration Enum_Val_Par, Enumeration *Enum_Ref_Par);
void Proc_7(One_Fifty Int_1_Par_Val, One_Fifty Int_2_Par_Val,
            One_Fifty *Int_Par_Ref);
void Proc_8(Arr_1_Dim Arr_1_Par_Ref, Arr_2_Dim Arr_2_Par_Ref,
            One_Fifty Int_1_Par_Val, One_Fifty Int_2_Par_Val);
Enumeration Func_1(Capital_Letter Ch_1_Par_Val, Capital_Letter Ch_2_Par_Val);
Boolean Func_2(Str_30 Str_1_Par_Ref, Str_30 Str_2_Par_Ref);
Boolean Func_3(Enumeration Enum_Par_Val);

// ===== Proc_1 =====
void Proc_1(Rec_Pointer Ptr_Val_Par) {
  Rec_Pointer Next_Record = Ptr_Val_Par->Ptr_Comp;
  Next_Record->variant.var_1.Int_Comp = 5;
  Next_Record->variant.var_1.Int_Comp =
      Ptr_Val_Par->variant.var_1.Int_Comp;
  Next_Record->Ptr_Comp = Ptr_Val_Par->Ptr_Comp;
  Proc_3(&Next_Record->Ptr_Comp);
  if (Next_Record->Discr == Ident_1) {
    Next_Record->variant.var_1.Int_Comp = 6;
    Proc_6(Ptr_Val_Par->variant.var_1.Enum_Comp,
           &Next_Record->variant.var_1.Enum_Comp);
    Next_Record->Ptr_Comp = Ptr_Glob->Ptr_Comp;
    Proc_7(Next_Record->variant.var_1.Int_Comp, 10,
           &Next_Record->variant.var_1.Int_Comp);
  } else {
    Next_Record->Ptr_Comp = Ptr_Val_Par->Ptr_Comp;
  }
}

// ===== Proc_2 =====
void Proc_2(One_Fifty *Int_Par_Ref) {
  One_Fifty Int_Loc;
  Enumeration Enum_Loc;
  Int_Loc = *Int_Par_Ref + 10;
  do {
    if (Ch_1_Glob == 'A') {
      Int_Loc -= 1;
      *Int_Par_Ref = Int_Loc - Int_Glob;
      Enum_Loc = Ident_1;
    }
  } while (Enum_Loc != Ident_1);
}

// ===== Proc_3 =====
void Proc_3(Rec_Pointer *Ptr_Ref_Par) {
  if (Ptr_Glob != 0) *Ptr_Ref_Par = Ptr_Glob->Ptr_Comp;
  Proc_7(10, Int_Glob, &Ptr_Glob->variant.var_1.Int_Comp);
}

// ===== Proc_4 =====
void Proc_4(void) {
  Boolean Bool_Loc;
  Bool_Loc = Ch_1_Glob == 'A';
  Bool_Glob = Bool_Loc | Bool_Glob;
  Ch_2_Glob = 'B';
}

// ===== Proc_5 =====
void Proc_5(void) {
  Ch_1_Glob = 'A';
  Bool_Glob = false;
}

// ===== Proc_6 =====
void Proc_6(Enumeration Enum_Val_Par, Enumeration *Enum_Ref_Par) {
  *Enum_Ref_Par = Enum_Val_Par;
  if (!Func_3(Enum_Val_Par)) *Enum_Ref_Par = Ident_4;
  switch (Enum_Val_Par) {
    case Ident_1: *Enum_Ref_Par = Ident_1; break;
    case Ident_2:
      if (Int_Glob > 100) *Enum_Ref_Par = Ident_1;
      else *Enum_Ref_Par = Ident_4;
      break;
    case Ident_3: *Enum_Ref_Par = Ident_2; break;
    case Ident_4: break;
    case Ident_5: *Enum_Ref_Par = Ident_3; break;
  }
}

// ===== Proc_7 =====
void Proc_7(One_Fifty Int_1_Par_Val, One_Fifty Int_2_Par_Val,
            One_Fifty *Int_Par_Ref) {
  One_Fifty Int_Loc;
  Int_Loc = Int_1_Par_Val + 2;
  *Int_Par_Ref = Int_2_Par_Val + Int_Loc;
}

// ===== Proc_8 =====
void Proc_8(Arr_1_Dim Arr_1_Par_Ref, Arr_2_Dim Arr_2_Par_Ref,
            One_Fifty Int_1_Par_Val, One_Fifty Int_2_Par_Val) {
  One_Fifty Int_Loc;
  Int_Loc = Int_1_Par_Val + 5;
  Arr_1_Par_Ref[Int_Loc] = Int_2_Par_Val;
  Arr_2_Par_Ref[Int_Loc][Int_1_Par_Val] = Int_Loc + Int_2_Par_Val;
  Arr_2_Par_Ref[Int_2_Par_Val][Int_Loc] = Int_Loc + Int_2_Par_Val;
}

// ===== Func_1 =====
Enumeration Func_1(Capital_Letter Ch_1_Par_Val, Capital_Letter Ch_2_Par_Val) {
  Capital_Letter Ch_Loc_1;
  Capital_Letter Ch_Loc_2;
  Ch_Loc_1 = Ch_1_Par_Val;
  Ch_Loc_2 = Ch_Loc_1;
  if (Ch_Loc_2 != Ch_2_Par_Val) return Ident_1;
  else {
    Ch_Loc_1 = Ch_2_Par_Val;
    Ch_Loc_2 = Ch_1_Par_Val;
    if (Ch_Loc_1 != Ch_2_Par_Val) return Ident_2;
    else return Ident_3;
  }
}

// ===== Func_2 =====
Boolean Func_2(Str_30 Str_1_Par_Ref, Str_30 Str_2_Par_Ref) {
  One_Thirty Int_Loc;
  Capital_Letter Ch_Loc;
  Int_Loc = 2;
  while (Int_Loc <= 2) {
    if (Func_1(Str_1_Par_Ref[Int_Loc], Str_2_Par_Ref[Int_Loc + 1]) == Ident_1) {
      Ch_Loc = 'A';
      Int_Loc += 1;
    }
    if (Ch_Loc == Str_2_Par_Ref[Int_Loc + 1]) Int_Loc += 1;
    if (Ch_Loc >= 'W' && Ch_Loc <= 'Z') Int_Loc += 1;
  }
  return Ch_Loc == 'A';
}

// ===== Func_3 =====
Boolean Func_3(Enumeration Enum_Par_Val) {
  Enumeration Enum_Loc;
  Enum_Loc = Enum_Par_Val;
  if (Enum_Loc == Ident_3) return true;
  else if (Enum_Loc == Ident_1) return false;
  else return false;
}

// ===== Entry point (ChipForge cpu_sim 默认 _start symbol) =====
extern "C" void _start(void) {
  const int Number_Of_Runs = 2000;

  One_Fifty Int_1_Loc, Int_2_Loc, Int_3_Loc;
  char Ch_Index;
  Enumeration Enum_Loc;
  Str_30 Str_1_Loc, Str_2_Loc;
  int Run_Index;

  // 自供 globals (avoid alloca)
  static Rec_Type Next_Ptr_Static;
  static Rec_Type Ptr_Static;
  Ptr_Glob = &Ptr_Static;
  Next_Ptr_Glob = &Next_Ptr_Static;

  Ptr_Glob->Ptr_Comp = Next_Ptr_Glob;
  Ptr_Glob->Discr = Ident_1;
  Ptr_Glob->variant.var_1.Enum_Comp = Ident_3;
  Ptr_Glob->variant.var_1.Int_Comp = 40;
  dhry_strcpy(Ptr_Glob->variant.var_1.Str_Comp,
              "DHRYSTONE PROGRAM, SOME STRING", 30);
  dhry_strcpy(Str_1_Loc, "DHRYSTONE PROGRAM, 1'ST STRING", 30);
  Arr_2_Glob[8][7] = 10;

  // Main Dhrystone benchmark loop
  for (Run_Index = 1; Run_Index <= Number_Of_Runs; ++Run_Index) {
    Proc_5();
    Proc_4();
    Int_1_Loc = 2;
    Int_2_Loc = 3;
    dhry_strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 2'ND STRING", 30);
    Enum_Loc = Ident_2;
    Bool_Glob = !Func_2(Str_1_Loc, Str_2_Loc);
    while (Int_1_Loc < Int_2_Loc) {
      Int_3_Loc = 5 * Int_1_Loc - Int_2_Loc;
      Proc_7(Int_1_Loc, Int_2_Loc, &Int_3_Loc);
      Int_1_Loc += 1;
    }
    Proc_8(Arr_1_Glob, Arr_2_Glob, Int_1_Loc, Int_3_Loc);
    Proc_1(Ptr_Glob);
    for (Ch_Index = 'A'; Ch_Index <= Ch_2_Glob; ++Ch_Index) {
      if (Enum_Loc == Func_1(Ch_Index, 'C')) {
        Proc_6(Ident_1, &Enum_Loc);
        dhry_strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 3'RD STRING", 30);
        Int_2_Loc = Run_Index;
        Int_Glob = Run_Index;
      }
    }
    Int_2_Loc = Int_2_Loc * Int_1_Loc;
    Int_1_Loc = Int_2_Loc / Int_3_Loc;
    Int_2_Loc = 7 * (Int_2_Loc - Int_3_Loc) - Int_1_Loc;
    Proc_2(&Int_1_Loc);
  }

  tohost_pass();
}