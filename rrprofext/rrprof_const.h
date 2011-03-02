#ifndef RRPROF_CONST
#define RRPROF_CONST




// 動作部分設定
#define ENABLE_TIME_GETTING         1
#define ENABLE_RUBY_RUNTIME_INFO    1
#define ENABLE_SEARCH_NODES         1
#define ENABLE_CALC_TIMES           1
#define ENABLE_STACK                1

// #define DO_NOTHING_IN_HOOK          1
// #define PRINT_FLAGS                 1

// #define PRINT_DEBUG                 1

// ENABLE_RUBY_RUNTIME_INFO depends on ENABLE_SEARCH_NODES

// 動作モード設定
#define TIMERMODE_CLOCK_GETTIME_MONOTONIC       1
//#define TIMERMODE_TIMERTHREAD                   1

// Timer threadのサンプリング間隔(usleep)
#define TIMERTHREAD_INTERVAL            100

// プロトコルの定数
#define MSG_START_PROFILE 2
#   define PM_METHODTABLE      1
#   define PM_CALLTREE         2

#define MSG_QUERY_INFO 3
#   define INFO_DATA_SLIDE  1

#define MSG_REQ_PROFILE_DATA    4    
#define MSG_SLIDE_INFO          5
#define MSG_PROFILE_DATA        6
#define MSG_QUERY_NAMES           7
#define QUERY_NAMES_SYMBOL    1
#define QUERY_NAMES_CLASS    2
#define MSG_NAMES               8
#define MSG_STACK_DATA        9

#define MSG_COMMAND_SUCCEED  100
#define MSG_ERROR            101

#define BT_PERFORMANCE      1
#define BT_STACK            2





// 検証
// ENABLE_RUBY_RUNTIME_INFO depends on ENABLE_SEARCH_NODES
#ifdef ENABLE_SEARCH_NODES
#   ifndef ENABLE_RUBY_RUNTIME_INFO
#       error "ENABLE_RUBY_RUNTIME_INFO depends on ENABLE_SEARCH_NODES"
#   endif
#endif

#if !TIMERMODE_CLOCK_GETTIME_MONOTONIC && !TIMERMODE_TIMERTHREAD
#   error "Set timer mode"
#endif




#endif // RRPROF_CONST
