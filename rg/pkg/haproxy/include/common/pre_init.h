#ifndef pre_init_h
#define pre_init_h

void __backend_init(void);
void ev_sepoll_do_register(void);
void __client_init(void);
void ev_poll_do_register(void);
void __dumpstats_module_init(void);
void __pattern_init(void);
void __acl_init(void);
void __http_protocol_init(void);
void __pipe_module_init(void);
void __proxy_module_init(void);
void ev_kqueue_do_register(void);
void __uxst_protocol_init(void);
void ev_select_do_register(void);
void __tcp_protocol_init(void);

#endif

