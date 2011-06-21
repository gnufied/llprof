
#include "llprof.h"
#include <string>
#include <cstdlib>
using namespace std;

bool g_aggressive_mode = true;

void llprof_init()
{
#ifdef LLPROF_DEBUG
    cout << "[llprof] Debug mode enabled." << endl;
#endif

    if(llprof_get_target_name() == "")
    {
        if(getenv("LLPROF_PROFILE_TARGET_NAME"))
        {
            llprof_set_target_name(getenv("LLPROF_PROFILE_TARGET_NAME"));
        }
        else
        {
            llprof_set_target_name("noname");
        }    
    }
    

    llprof_calltree_init();
    llprof_start_server();
    if(g_aggressive_mode)
    {
        llprof_start_aggressive_thread();
    }
}



// アグレッシブモードの設定
void llprof_aggressive(bool enabled)
{
    g_aggressive_mode = enabled;
}
