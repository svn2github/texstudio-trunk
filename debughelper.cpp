#include "debughelper.h"
#include "mostQtHeaders.h"
#include "QMutex"

#if (defined(x86_64) || defined(__x86_64__))
#define CPU_IS_64
#elif (defined(ppc) || defined(__ppc__))
#define CPU_IS_PPC
#else
#define CPU_IS_32
#endif

//===========================Abstract CPU model==========================

//platfom independent implementation of assembler instructions 
//(use case: before changing eip, the old eip must be pushed in the correct way on the stack, otherwise the 
//           backtrace doesn't show the function that actually crashed)
#define CALL_INSTRUCTION_SIZE 0
struct SimulatedCPU {
	char * pc; //e.g. eip
	char * frame; //e.g. ebp
	char * stack; //e.g. esp
	
	inline void push(char * value){
		stack -= sizeof(void*); 
		*(char**)(stack) = value; 
	}
	inline char* pop(){
		char* temp = *(char**)(stack);
		stack += sizeof(void*); 
		return temp;
	}

	inline void jmp(char * value){
		pc = value;
	}
	inline void call(char * value){
		push(pc + CALL_INSTRUCTION_SIZE);
		jmp(value);
	}
	inline void ret(){
		pc = pop();
	}
	inline void enter(int size){
		push(frame);
		frame = stack;
		stack -= size;
	}
	inline void leave(){
		stack = frame;
		frame = pop();
		ret();
	}
	
	template<typename T> void set_all(const T& pc, const T& frame, const T& stack){
		this->pc = (char*)(pc);
		this->frame = (char*)(frame);
		this->stack = (char*)(stack);
	}
	template<typename T> void get_all(T& pc, T& frame, T& stack){
		pc = (T)(this->pc);
		frame = (T)(this->frame);
		stack = (T)(this->stack);
	}
	

	bool stackWalk(){
		leave();
		pc-=CALL_INSTRUCTION_SIZE;
	//	fprintf(stderr, "%p (at %p), %p (at %p), %p\n", *(char**)(frame),frame, *(char**)(stack), stack, pc);
		return frame >= stack && frame && stack;
	}
	void unwindStack(){
		SimulatedCPU temp = *this;
		int frames = 0; 
		while (temp.stackWalk()) frames++;
		if (frames == 0) return;
		if (frames < 100) frames = frames - 10;
		else frames = frames - 100;
		while (frames > 0) { this->leave(); frames--; }
	}	
};


//===========================STACK TRACE PRINTING=========================
void print_message(const char* title, const char *assertion, const char *file, int line, const char* end){
#ifdef Q_WS_WIN
	qDebug("%s %s in %s: %i\r\n%s", title, assertion, file, line, end);
#else
	fprintf(stderr, "%s %s in %s: %i\r\n%s", title, assertion, file, line, end);
#endif
	qDebug("%s %s in %s: %i\r\n%s", title, assertion, file, line, end);
}

#ifdef unix
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

void print_backtrace(const char* title, const char * assertion, const char * file, int line){ //from http://stackoverflow.com/questions/3151779/how-its-better-to-invoke-gdb-from-program-to-print-its-stacktrace
	print_message(title, assertion, file, line,"");
	char pid_buf[30];
	sprintf(pid_buf, "%d", getpid());
	char name_buf[512];
	name_buf[readlink("/proc/self/exe", name_buf, 511)]=0;
	int child_pid = fork();
	if (!child_pid) {
		dup2(2,1); // redirect output to stderr
		fprintf(stdout,"stack trace for %s pid=%s\n",name_buf,pid_buf);
		execlp("gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "bt", name_buf, pid_buf, NULL);
		abort(); /* If gdb failed to start */
	} else {
		waitpid(child_pid,NULL,0);
	}
}
#elif defined(Q_WS_WIN)
#include <QSysInfo>
#include "windows.h"

//from wine
//dbghelp.h
typedef BOOL WINAPI (*SymInitializeFunc)(HANDLE, PCSTR, BOOL);
typedef PVOID WINAPI (*SymFunctionTableAccess64Func)(HANDLE, DWORD64);
typedef DWORD64 WINAPI (*SymGetModuleBase64Func)(HANDLE, DWORD64);
typedef enum
{
	AddrMode1616,
	AddrMode1632,
	AddrModeReal,
	AddrModeFlat
} ADDRESS_MODE;
//
// New KDHELP structure for 64 bit system support.
// This structure is preferred in new code.
//
typedef struct _KDHELP64 {
	
	//
	// address of kernel thread object, as provided in the
	// WAIT_STATE_CHANGE packet.
	//
	DWORD64   Thread;
	
	//
	// offset in thread object to pointer to the current callback frame
	// in kernel stack.
	//
	DWORD   ThCallbackStack;
	
	//
	// offset in thread object to pointer to the current callback backing
	// store frame in kernel stack.
	//
	DWORD   ThCallbackBStore;
	
	//
	// offsets to values in frame:
	//
	// address of next callback frame
	DWORD   NextCallback;
	
	// address of saved frame pointer (if applicable)
	DWORD   FramePointer;
	
	
	//
	// Address of the kernel function that calls out to user mode
	//
	DWORD64   KiCallUserMode;
	
	//
	// Address of the user mode dispatcher function
	//
	DWORD64   KeUserCallbackDispatcher;
	
	//
	// Lowest kernel mode address
	//
	DWORD64   SystemRangeStart;
	
	DWORD64  Reserved[8];
	
} KDHELP64, *PKDHELP64;

typedef struct _tagADDRESS64 {
	DWORD64       Offset;
	WORD          Segment;
	ADDRESS_MODE  Mode;
} ADDRESS64, *LPADDRESS64;

typedef struct _STACKFRAME64
{
	ADDRESS64   AddrPC;
	ADDRESS64   AddrReturn;
	ADDRESS64   AddrFrame;
	ADDRESS64   AddrStack;
	ADDRESS64   AddrBStore;
	PVOID       FuncTableEntry;
	DWORD64     Params[4];
	BOOL        Far;
	BOOL        Virtual;
	DWORD64     Reserved[3];
	KDHELP64    KdHelp;
} STACKFRAME64, *LPSTACKFRAME64;

typedef BOOL (CALLBACK *PREAD_PROCESS_MEMORY_ROUTINE64)(HANDLE, DWORD64, PVOID, DWORD, PDWORD);
typedef PVOID (CALLBACK *PFUNCTION_TABLE_ACCESS_ROUTINE64)(HANDLE, DWORD64);
typedef DWORD64 (CALLBACK *PGET_MODULE_BASE_ROUTINE64)(HANDLE, DWORD64);
typedef DWORD64 (CALLBACK *PTRANSLATE_ADDRESS_ROUTINE64)(HANDLE, HANDLE, LPADDRESS64);
typedef BOOL WINAPI (*StackWalk64Func)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID,
                                       PREAD_PROCESS_MEMORY_ROUTINE64,
                                       PFUNCTION_TABLE_ACCESS_ROUTINE64,
                                       PGET_MODULE_BASE_ROUTINE64,
                                       PTRANSLATE_ADDRESS_ROUTINE64);

typedef struct _IMAGEHLP_SYMBOL64
{
	DWORD                       SizeOfStruct;
	DWORD64                     Address;
	DWORD                       Size;
	DWORD                       Flags;
	DWORD                       MaxNameLength;
	CHAR                        Name[1];
} IMAGEHLP_SYMBOL64, *PIMAGEHLP_SYMBOL64;

typedef struct _IMAGEHLP_LINE64
{
	DWORD                       SizeOfStruct;
	PVOID                       Key;
	DWORD                       LineNumber;
	PCHAR                       FileName;
	DWORD64                     Address;
} IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;

typedef BOOL WINAPI (*SymGetSymFromAddr64Func)(HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64);
typedef BOOL WINAPI (*SymGetLineFromAddr64Func)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);

QMutex backtraceMutex;

#define LOAD_FUNCTION(name, ansiname) static name##Func name = (name##Func)GetProcAddress(dbghelp, ansiname);
#define LOAD_FUNCTIONREQ(name, ansiname) LOAD_FUNCTION(name,ansiname) if (!name) return "failed to load function: " #name;
//from http://jpassing.com/2008/03/12/walking-the-stack-of-the-current-thread/
//     http://www.codeproject.com/Articles/11132/Walking-the-callstack
//alternative: __builtin_return_address, but it is said to not work so well
QString getBacktrace(){
	if (!backtraceMutex.tryLock()) return "locked";
	
	//init crap
	QStringList result;
	HANDLE process =  GetCurrentProcess();
	HANDLE thread = GetCurrentThread();
	
	static HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
	if (!dbghelp) return "failed to load dbghelp.dll";
	
	LOAD_FUNCTIONREQ(SymInitialize, "SymInitialize");
	LOAD_FUNCTIONREQ(SymGetModuleBase64, "SymGetModuleBase64");
	LOAD_FUNCTIONREQ(SymFunctionTableAccess64, "SymFunctionTableAccess64");
	LOAD_FUNCTIONREQ(StackWalk64, "StackWalk64");
	LOAD_FUNCTIONREQ(SymGetSymFromAddr64, "SymGetSymFromAddr64");
	LOAD_FUNCTION(SymGetLineFromAddr64, "SymGetLineFromAddr64");
	
	if (!(*SymInitialize)(((QSysInfo::windowsVersion() & QSysInfo::WV_DOS_based) == 0)?process:(HANDLE)GetCurrentProcessId(), 0, true))
		result << "Failed to initialize SymInitialize " << QString::number(GetLastError());
	
	CONTEXT context;
	ZeroMemory( &context, sizeof( CONTEXT ) );
	STACKFRAME64 stackFrame;
#if (defined(x86_64) || defined(__x86_64__))
	RtlCaptureContext( &context );
#else
	// Those three registers are enough.
geteip:
	context.Eip = (DWORD)&&geteip;
	__asm__(
	"mov %%ebp, %0\n"
	"mov %%esp, %1"
	: "=r"(context.Ebp), "=r"(context.Esp));
#endif
	ZeroMemory( &stackFrame, sizeof( stackFrame ) );
#ifdef CPU_IS_64
	DWORD machineType           = IMAGE_FILE_MACHINE_AMD64;
	stackFrame.AddrPC.Offset    = context.Rip;
	stackFrame.AddrPC.Mode      = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context.Rbp;//changed from rsp. correctly?
	stackFrame.AddrFrame.Mode   = AddrModeFlat;
	stackFrame.AddrStack.Offset = context.Rsp;
	stackFrame.AddrStack.Mode   = AddrModeFlat;
#else
	DWORD machineType           = IMAGE_FILE_MACHINE_I386;
	stackFrame.AddrPC.Offset    = context.Eip;
	stackFrame.AddrPC.Mode      = AddrModeFlat;
	stackFrame.AddrFrame.Offset = context.Ebp;
	stackFrame.AddrFrame.Mode   = AddrModeFlat;
	stackFrame.AddrStack.Offset = context.Esp;
	stackFrame.AddrStack.Mode   = AddrModeFlat;
	/* #elif _M_IA64
    MachineType                 = IMAGE_FILE_MACHINE_IA64;
    StackFrame.AddrPC.Offset    = Context.StIIP;
    StackFrame.AddrPC.Mode      = AddrModeFlat;
    StackFrame.AddrFrame.Offset = Context.IntSp;
    StackFrame.AddrFrame.Mode   = AddrModeFlat;
    StackFrame.AddrBStore.Offset= Context.RsBSP;
    StackFrame.AddrBStore.Mode  = AddrModeFlat;
    StackFrame.AddrStack.Offset = Context.IntSp;
    StackFrame.AddrStack.Mode   = AddrModeFlat;
  #else
    #error "Unsupported platform"*/
#endif
	
	
	//get stackframes
	QList<DWORD64> stackFrames;
	while ((*StackWalk64)(machineType, process, thread, &stackFrame, &context, 0, SymFunctionTableAccess64, SymGetModuleBase64, 0))
		stackFrames << stackFrame.AddrPC.Offset;
	
	
	_IMAGEHLP_SYMBOL64* symbol = (_IMAGEHLP_SYMBOL64*)malloc(sizeof(_IMAGEHLP_SYMBOL64) + 256);
	for (int i=0;i<stackFrames.size();i++) {
		DWORD64 displacement = 0;
		DWORD displacement32 = 0;
		ZeroMemory(symbol, sizeof(_IMAGEHLP_SYMBOL64));
		symbol->SizeOfStruct = sizeof(_IMAGEHLP_SYMBOL64);
		symbol->MaxNameLength = 256;
		
		_IMAGEHLP_LINE64 line;
		
		QString cur = QString::number(stackFrames[i], 16)+": ";
		
		if ((*SymGetSymFromAddr64)(process, stackFrames[i], &displacement, symbol))
			cur += QString::fromLocal8Bit(symbol->Name)+"+"+QString::number(displacement);
		else
			cur += "??? error: " + QString::number(GetLastError());
		
		
		if (SymGetLineFromAddr64) {
			if ((*SymGetLineFromAddr64)(process, stackFrames[i], &displacement32, &line))
				cur += " in " + QString::fromLocal8Bit(line.FileName)+":"+QString::number(line.LineNumber);
		}
		
		result << cur;
	}
	
	backtraceMutex.unlock();
	
	return result.join("\r\n");
}

void print_backtrace(const char* title, const char *assertion, const char *file, int line){
	print_message(title, message, file, line);
	QString bt = getBacktrace();
	static int count = 1;
	QFile tf(QDir::tempPath() + QString("/texstudio_backtrace%1.txt").arg(count++));
	if (tf.open(QFile::WriteOnly)){
		tf.write(QString("%1 %2 in %3: %4\r\n").arg(title).arg(assertion).arg(file).arg(line).toLocal8Bit());
		tf.write(bt.toLocal8Bit());
		tf.close();
	};
	qDebug() << bt;
	fprintf(stderr, "%s\r\n", qPrintable(bt));
}
#else //unknown os/mac
void print_backtrace(const char* title, const char * assertion, const char * file, int line){
	fprintf(stderr, "Unknown OS");
}
#endif

void print_backtrace(const QString& message){
	print_backtrace(message.toLocal8Bit().data(),"","",0);
}







//===========================CRASH HANDLER==============================



#ifdef Q_WS_WIN
#define SAFE_INT int
#else
#define SAFE_INT volatile sig_atomic_t
#endif

SAFE_INT crashHandlerType = 1;
SAFE_INT lastErrorWasAssert = 0;

#define CRASH_HANDLER_RECOVER 1
#define CRASH_HANDLER_PRINT_BACKTRACE 2


//=========================POSIX SIGNAL HANDLER===========================



#ifdef linux
#include "signal.h"
#include "ucontext.h"
#include "sys/ucontext.h"
#define USE_SIGNAL_HANDLER
#ifdef CPU_IS_64
#define PC_FROM_UCONTEXT(context) context->uc_mcontext.gregs[REG_RIP]
#define STACK_FROM_UCONTEXT(context) context->uc_mcontext.gregs[REG_RSP]
#define FRAME_FROM_UCONTEXT(context) context->uc_mcontext.gregs[REG_RBP]
#else
#define PC_FROM_UCONTEXT(context) context->uc_mcontext.gregs[REG_EIP]
#define STACK_FROM_UCONTEXT(context) context->uc_mcontext.gregs[REG_ESP]
#define FRAME_FROM_UCONTEXT(context) context->uc_mcontext.gregs[REG_EBP]
#endif
#endif

#ifdef Q_WS_MACX
#include "sys/signal.h"
#include "sys/ucontext.h"
#define USE_SIGNAL_HANDLER
//names from http://google-glog.googlecode.com/svn-history/r75/trunk/m4/pc_from_ucontext.m4
//for mac <= 10.4/tiger: if __ss.__ doesn't compile, replace it by ss.
#ifdef CPU_IS_64
#define PC_FROM_UCONTEXT(context) context->__ss.__rip
#define STACK_FROM_UCONTEXT(context) context->__ss.__rsp
#define FRAME_FROM_UCONTEXT(context) context->__ss.__rbp
#elif defined(CPU_IS_PPC)
#define PC_FROM_UCONTEXT(context) context->__ss.__srr0
#error need ppc stack register name
#else
#define PC_FROM_UCONTEXT(context) context->__ss.__eip
#define STACK_FROM_UCONTEXT(context) context->__ss.__esp
#define FRAME_FROM_UCONTEXT(context) context->__ss.__ebp
#endif
#endif

#ifdef USE_SIGNAL_HANDLER
volatile sig_atomic_t lastCrashSignal = 0;

#define SIGMYSTACKSEGV 64

const char * signalIdToName(int id){
	switch (id){
	case SIGSEGV: return ("SIGSEGV"); 
	case SIGFPE: return ("SIGFPE"); 
	case SIGMYSTACKSEGV: return ("SIGSEGV on stack"); 
	default: return ("SIG???"); 
	}
}

void signalHandler(int type, siginfo_t * si, void* ccontext){
	lastErrorWasAssert = 0;
	SimulatedCPU cpu;
	if (ccontext) {
		ucontext_t* context = static_cast<ucontext_t*>(ccontext);
		cpu.set_all(PC_FROM_UCONTEXT(context), FRAME_FROM_UCONTEXT(context), STACK_FROM_UCONTEXT(context));

		char *addr = (char*)(si->si_addr);
		char * minstack = cpu.stack < cpu.frame ? cpu.stack : cpu.frame;
		quintptr distance = addr < minstack ? minstack - addr : addr - minstack;
		if (distance < 1024) type = SIGMYSTACKSEGV;
	}
	if (crashHandlerType & CRASH_HANDLER_PRINT_BACKTRACE || !ccontext) {
		print_backtrace(signalIdToName(type),"","",0); 
		if (!ccontext) return;
	}
	if (crashHandlerType & CRASH_HANDLER_RECOVER) {
		lastCrashSignal = type;
		if (type == SIGMYSTACKSEGV) cpu.unwindStack();
		cpu.call((char*)(&recover));

		ucontext_t* context = static_cast<ucontext_t*>(ccontext);
		cpu.get_all(PC_FROM_UCONTEXT(context), FRAME_FROM_UCONTEXT(context), STACK_FROM_UCONTEXT(context));		              
	}
}

const int SIGNAL_STACK_SIZE = 256*1024;
char staticSignalStack[SIGNAL_STACK_SIZE];

void registerCrashHandler(int mode){
	crashHandlerType = mode;
	if (mode >= 0) {
		stack_t ss;
		ss.ss_sp = staticSignalStack;
		ss.ss_size = SIGNAL_STACK_SIZE;
		ss.ss_flags = 0;
		sigaltstack(&ss, 0);
		
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa)); sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
		sa.sa_sigaction = &signalHandler; sigaction(SIGSEGV, &sa, 0);
		sa.sa_sigaction = &signalHandler; sigaction(SIGFPE, &sa, 0);
	}
}

QString getLastCrashInformationInternal(){
	return signalIdToName(lastCrashSignal);
}

#endif







//=========================WINDOWS EXCEPTION HANDLER===========================
#ifdef Q_WS_WIN

#define CATCHED_EXCEPTIONS(X)  \
	X(EXCEPTION_ACCESS_VIOLATION) \
	X(EXCEPTION_ARRAY_BOUNDS_EXCEEDED) \
	X(EXCEPTION_DATATYPE_MISALIGNMENT) \
	X(EXCEPTION_FLT_DENORMAL_OPERAND) \
	X(EXCEPTION_FLT_DIVIDE_BY_ZERO) \
	X(EXCEPTION_FLT_INEXACT_RESULT) \
	X(EXCEPTION_FLT_INVALID_OPERATION) \
	X(EXCEPTION_FLT_OVERFLOW) \
	X(EXCEPTION_FLT_UNDERFLOW) \
	X(EXCEPTION_IN_PAGE_ERROR) \
	X(EXCEPTION_INT_DIVIDE_BY_ZERO) \
	X(EXCEPTION_INT_OVERFLOW) \
	X(EXCEPTION_INVALID_DISPOSITION)
//EXCEPTION_FLT_STACK_CHECK, EXCEPTION_STACK_OVERFLOW
//EXCEPTION_ILLEGAL_INSTRUCTION
//EXCEPTION_NONCONTINUABLE_EXCEPTION
//EXCEPTION_PRIV_INSTRUCTION
#ifdef CPU_IS_64
#define PC_FROM_UCONTEXT(context) (context)->Rip
#define FRAME_FROM_UCONTEXT(context) (context)->Rbp
#define STACK_FROM_UCONTEXT(context) (context)->Rsp
#else
#define PC_FROM_UCONTEXT(context) (context)->Eip
#define FRAME_FROM_UCONTEXT(context) (context)->Ebp
#define STACK_FROM_UCONTEXT(context) (context)->Esp
#endif

const char* exceptionCodeToName(int code){
	switch (code){
#define X(t) case t: return #t;
	CATCHED_EXCEPTIONS(X)
              #undef X
	}
	return "unknown";
}


int lastExceptionCode = 0;
quintptr lastExceptionAddress = 0;

LONG WINAPI crashHandler(_EXCEPTION_POINTERS *ExceptionInfo) {
	if (!ExceptionInfo) return EXCEPTION_CONTINUE_SEARCH;
	bool isCatched = false;
#define X(t) if (ExceptionInfo->ExceptionRecord->ExceptionCode == t) isCatched = true;
	CATCHED_EXCEPTIONS(X)
              #undef X
	              if (!isCatched) return EXCEPTION_CONTINUE_SEARCH;
	
	lastErrorWasAssert = 0;
              
	if (crashHandlerType == 1) {
		lastExceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
		
		lastExceptionAddress = quintptr(PC_FROM_UCONTEXT(ExceptionInfo->ContextRecord));
		
		*(void**)(&PC_FROM_UCONTEXT(ExceptionInfo->ContextRecord)) = (void*)(&recover);
		return EXCEPTION_CONTINUE_EXECUTION;
	} else {
		txs_assert(exceptionCodeToName(ExceptionInfo->ExceptionRecord->ExceptionCode), "", 0);
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

void registerCrashHandler(int mode){
	crashHandlerType = mode;
	if (mode >= 0) {
		SetUnhandledExceptionFilter(&crashHandler);
	}
}

QString getLastCrashInformationInternal(){
	return QString::fromLocal8Bit(exceptionCodeToName(lastExceptionCode)) + " at " + QString::number(lastExceptionAddress,16);
}
#endif





//========================NEW ASSERT==============================


#undef qt_assert
Q_CORE_EXPORT void qt_assert(const char *assertion, const char *file, int line);

QString lastAssert;

void txs_assert(const char *assertion, const char *file, int line){
	print_message("Assertion failure: ", assertion, file, line, "Prepare to print backtrace:\r\n");
	print_backtrace("Assertion failure (repeated): ", assertion, file, line);

	lastErrorWasAssert = 1;
	lastAssert = QString("Assert failure: %1 in %2:%3").arg(assertion).arg(file).arg(line);
	recover();
	//won't be called:
	qt_assert(assertion, file, line);
	exit(1);
}

QString getLastCrashInformation(){
	if (lastErrorWasAssert) return lastAssert;
	else return getLastCrashInformationInternal();
}


