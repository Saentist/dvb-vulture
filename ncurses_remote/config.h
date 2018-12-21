#ifndef __config_h
#define __config_h
#define LOC_CFG_PFX "~/.dvbvulture"
#define LOC_CFG (LOC_CFG_PFX "/nr.conf")
#define DEFAULT_AF AF_INET
#define DEFAULT_SERVER4 "192.168.0.3"
#define DEFAULT_SERVER6 "fe80::3"
#define DEFAULT_SERVER(af_) (((af_)==AF_INET)?DEFAULT_SERVER4:DEFAULT_SERVER6)

#ifdef __WIN32__
#define K_BACK VK_BACK
#else
#define K_BACK 27
#endif

#endif
