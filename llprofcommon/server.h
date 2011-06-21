/* server.h - モニタから接続されるためのサーバースレッド
 *  
 */
#ifndef SERVER_H
#define SERVER_H



// サーバースレッドの開始
void llprof_start_server();

// アグレッシブスレッドの開始
void llprof_start_aggressive_thread();

void SendMessage(int sock, int msg_id, void *buf, int buf_sz);
void SendMessage2(int sock, int msg_id, void *buf1, int buf_sz1, void *buf2, int buf_sz2);

void llprof_set_target_name(const string &name);
string llprof_get_target_name();




#endif
