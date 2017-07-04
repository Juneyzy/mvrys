#ifndef __JSON_HDR_H__
#define __JSON_HDR_H__

struct json_hdr {
	int ver;
	int dir;
	int cmd;
	uint32_t seq;
};

#define JSON_DEBUG
#define json_printf(fmt,args...) printf(fmt,##args)

static inline void
web_json_type_parse(
    json_object *jso,
    void *data, size_t dsize)
{
    json_bool jb;
    int32_t ji;
    const char *str;

    switch(json_object_get_type(jso)){
        case json_type_boolean:
            jb = json_object_get_boolean(jso);
            *(json_bool *)data = jb;
            break;
        case json_type_string:
            str = json_object_get_string(jso);
            memcpy(data, str, dsize > strlen(str)? strlen(str):dsize);
            break;
        case json_type_int:
            ji = json_object_get_int(jso);
            *(int32_t *)data = ji;
            break;
         default:
            break;
    }

    return;
}

static inline json_object *
web_json_to_field(
    json_object *new_obj,
    const char *field)
{
	json_object *result = NULL;
	
	json_object_object_get_ex (new_obj, field, &result);

	return result;
}

#define json_field(cobj, field, variable, variable_len) web_json_type_parse(\
    web_json_to_field(cobj, field),\
        variable, variable_len);

static inline json_object *
web_json_tokener_parse(const char *desc,
	const char *json_str)
{
    enum json_tokener_error errors;

    json_object *jo = json_object_new_object();
    jo = json_tokener_parse_verbose(json_str, &errors);
    rt_log_notice ("%s.toString(%d, %d) >> %s", 
        				desc, (int)strlen(json_str), errors, json_object_to_json_string(jo));
    return jo;
}

static inline void
web_json_element(struct json_hdr *jhdr)
{
#ifdef JSON_DEBUG
	json_printf("\tversion=\"%d\"\n", jhdr->ver);
	json_printf("\tcommand=%d\n", jhdr->cmd);
	json_printf("\tdirection=%d\n", jhdr->dir);
	json_printf("\tsequence=%u\n", jhdr->seq);
#endif
}

static inline void
web_json_parser (const char *jstr, struct json_hdr *jhdr,
				json_object **injson)
{
	json_object *head;

	*injson = web_json_tokener_parse("INBOUND", jstr);

	head = web_json_to_field (*injson, "head");

	json_field(head, "version", (void*)&jhdr->ver, sizeof(typeof(jhdr->ver)));
	json_field(head, "direct", (void*)&jhdr->dir, sizeof(typeof(jhdr->dir)));
	json_field(head, "cmd", (void*)&jhdr->cmd, sizeof(typeof(jhdr->cmd)));
	json_field(head, "no", (void*)&jhdr->seq, sizeof(typeof(jhdr->seq)));
}

static inline  void
web_json_head_add(json_object *head,
			    struct json_hdr *jhdr, json_object *body)
{
	json_object *hdr = json_object_new_object();
	
	json_object_object_add(hdr,   "version", json_object_new_int(jhdr->ver));
	json_object_object_add(hdr,   "direct", json_object_new_int(1));
	json_object_object_add(hdr,   "cmd", json_object_new_int(jhdr->cmd));
	json_object_object_add(hdr,   "no", json_object_new_int64(jhdr->seq));

	json_object_object_add(head,    "head",     hdr);

	json_object_object_add(head,    "msg",     body);

	return;
}
#if 0
static inline int  web_json_register(int web_sock, int no)
{
	int    ret = -1;
	char buf[1024] = {0}, *regstr;
	
	struct json_object *object, *msg, *head;

	object = json_object_new_object();
	head = json_object_new_object();
	msg = json_object_new_object();

	json_object_object_add(head, "cmd", json_object_new_int(SG_X_REG));
	json_object_object_add(head, "direct", json_object_new_int(1));
	json_object_object_add(head, "no", json_object_new_int(no));
	json_object_object_add(head, "version", json_object_new_int(1));

	json_object_object_add(msg, "program", json_object_new_string("vrs"));

	json_object_object_add(object, "head", head);
	json_object_object_add(object, "msg", msg);
	
	regstr = (char *)json_object_to_json_string(object);
	json_send (web_sock, regstr, strlen(regstr), web_json_data_rebuild);

	ret = read(web_sock, buf, 1024);
	if(ret <= 0)
	{
		return -1;
	}

	web_json_tokener_parse ("Response", buf);
	
	return 0;
}
#endif

/** Java will not get data without '\n' */
static inline int
web_json_data_rebuild(const char *data, 
    size_t size, 
    char **odata, 
    size_t *osize)
{
    size_t real_size = size + 2; /** 2, '\n' + '\0' */
    
    if (!data || !size) 
        return -1;
    
    *odata = malloc (real_size);
    assert(*odata);
    memset (*odata, 0, real_size);
    snprintf(*odata, real_size,"%s\n", data);
    *osize = real_size;
    
    return 0;
}

static inline ssize_t json_send(int sock, const char *data, size_t size, 
    int(*rebuild)(const char *iv, size_t is, char **ov, size_t *os))
{
    char *odata = NULL;
    size_t osize = 0;
    int xret = -1;
   
    if (rebuild){
        xret = rebuild(data, size, &odata, &osize);
        if (xret < 0)
            goto finish;

        xret = rt_sock_send (sock, odata, osize);
	web_json_tokener_parse ("OUTBOUND", data);
        kfree (odata);
    }

finish:
    return xret;
}

#endif

