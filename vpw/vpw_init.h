#ifndef __VRS_YAML_CONF_H__
#define __VRS_YAML_CONF_H__

extern void vpw_conf_init();

extern int load_private_vpw_config(int reload);

extern int load_public_config(int reload);

extern int load_rule_and_model(int reload);

#endif /* __VRS_YAML_CONF_H__ */

