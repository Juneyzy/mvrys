#ifndef __RT_BUG_H__
#define __RT_BUG_H__


#define __WARN()		__WARN_TAINT(TAINT_WARN)
#define __WARN_printf(arg...)	do { printk(arg); __WARN(); } while (0)
#define __WARN_printf_taint(taint, arg...)				\
	do { printf(arg); __WARN_TAINT(taint); } while (0)
	

#ifndef WARN_ON
#define WARN_ON(condition) ({					\
	int __ret_warn_on = !!(condition);				\
	if (unlikely(__ret_warn_on))					\
		__WARN();						              \
	unlikely(__ret_warn_on);					       \
})
#endif

#ifndef WARN
#define WARN(condition, format...) ({			       \
	int __ret_warn_on = !!(condition);				\
	if (unlikely(__ret_warn_on))					\
		__WARN_printf(format);					\
	unlikely(__ret_warn_on);					       \
})
#endif


#define WARN_TAINT(condition, taint, format...) ({	\
	int __ret_warn_on = !!(condition);				\
	if (unlikely(__ret_warn_on))					\
		__WARN_printf_taint(taint, format);		\
	unlikely(__ret_warn_on);					       \
})


#endif
