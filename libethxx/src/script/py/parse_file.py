#!/usr/bin/env python

import string
import sys,os
import common
#import socket, struct

def type_help():
    print "TYPE: 1 - http"
    print "      2 - email"
    print "      3 - tftp"
    print "      4 - im"

def call_func(protocol):
    prot_dict = {"http":(parse_http_file, "http_lib_table"),
            "email":(parse_email_file, "email_lib_table"),
            "tftp":(parse_tftp_file, "tftp_lib_table"),
            "im":(parse_im_file, "im_lib_table")
            }
    
    if protocol in prot_dict.keys():
        return prot_dict[protocol]
    else:
        print "Protocol {0:s} not support yet!".format(protocol)
        return (None, "")
    
    
def parse_list_to_dict(contents, dctn):
    if not isinstance(dctn, dict):
        print dctn, "is not a dictionary"
        return False
    for line in contents:
        line = line.strip()
        if len(line) != 0 and line.find(':') != -1:
            line_list = line.split(":", 1)
            dctn[line_list[0].strip()] = line_list[1].strip()

    return True

def parse_file_to_dict(filename, dctn):
    if not os.path.isfile(filename):
        print  filename, " is not a file!"
        return False
    try:
        fp = open(filename, 'r')
        contents = fp.readlines()
        fp.close()
    except IOError as e:
        err = str(e)
        if err.find("Too many open files"):
            print "open {0:s} failed, too many open files".format(filename)
        return False
    return parse_list_to_dict(contents, dctn)


def parse_common_info(dctn):

    keys = dctn.keys()

    if "ctm" in keys:
        ctm = dctn["ctm"]
    else:
        ctm = -1

    if "sip" in keys:
        sip_str = dctn["sip"]
        #sip = socket.ntohl(struct.unpack('I', socket.inet_aton(sip_str))[0])
    else:
        sip = ""

    if "sp" in keys:
        sp = string.atoi(dctn["sp"])
    else:
        sp = 0

    if "dip" in keys:
        dip_str = dctn["dip"]
        #dip = socket.ntohl(struct.unpack('I', socket.inet_aton(dip_str))[0])
    else:
        dip = ""

    if "dp" in keys:
        dp = string.atoi(dctn["dp"])
    else:
        dp = 0

    if "SRC_ACCOUNT" in keys:
        src_account = dctn["SRC_ACCOUNT"]
    else:
        src_account = ""

    if "DST_ACCOUNT" in keys:
        dst_account = dctn["DST_ACCOUNT"]
    else:
        dst_account = ""

    if "type" in keys:
        #type_num = string.atoi(dctn["type"])
        type_str = dctn["type"]
    else:
        #type_num = -1
        type_str = ""

    if "fnm" in keys:
        prefix = common.SERVERPATH_PREFIX
        protdir = dctn["protocol"]
        fnm = dctn["fnm"]
        details = os.path.join(prefix, protdir, fnm)
    else:
        details = ""
        
    if "Attachment-Path" in dctn.keys():
        tmp = dctn["Attachment-Path"]
        if len(tmp):
            attach_path = os.path.join(prefix, protdir, tmp)
    else:
        attach_path = ""

    if "Attachment" in dctn.keys():
        attachment = dctn["Attachment"]
        #debug
        #if len(tmp):
        #    attachment = os.path.join(prefix, protdir, attach_path, attachment)
    else:
        attachment = ""
    
    values = (ctm, sip_str, sp, dip_str, dp, src_account, dst_account, type_str, details, attach_path, attachment)
    return values


def parse_http_file(dctn):
    "parser dict and return tuple"
    flag = 0
    (ctm, sip, sp, dip, dp, src_account, dst_account, type_num, details, attach_path, attachment) = parse_common_info(dctn)
    
    type_dict = {"0":"none", "1":"GET", "2":"POST"}
    try:
        type_str = type_dict[type_num]
    except KeyError:
        type_str = type_num

    if "fnm" in dctn.keys() and len(dctn["fnm"]) != 0:
        spec_dict = {}
        dataname = os.path.basename(dctn["fnm"])
        filepath = os.path.join(common.SHARE_PATH, dataname)
        ret = parse_file_to_dict(filepath, spec_dict)

        if ret:
            keys = spec_dict.keys()

            if "Referer" in keys:
                url = spec_dict["Referer"]
            else:
                url = ""

            if "Content-Length" in keys:
                file_size = string.atoi(spec_dict["Content-Length"])
            else:
                file_size = 0
        else:
            flag = 1

    else:
        flag = 1

    if flag == 1:
        url = ""
        file_size = 0

    values = (ctm, sip, sp, dip, dp, src_account, dst_account, type_str, url, file_size, details)
    return values

def parse_email_file(dctn):
    flag = 0
    (ctm, sip, sp, dip, dp, src_account, dst_account, type_num, details, attach_path, attachment) = parse_common_info(dctn)
    
    
    type_dict = {"0":"none", "1":"SMTP", "2":"POP3", "3":"IMAP"}
    try:
        type_str = type_dict[type_num]
    except KeyError:
        print "email: error occour:", e.message
        type_str = type_num

    if "fnm" in dctn.keys():
        spec_dict = {}
        dataname = os.path.basename(dctn["fnm"])
        filepath = os.path.join(common.SHARE_PATH, dataname)
        ret = parse_file_to_dict(filepath, spec_dict)

        if ret:
            keys = spec_dict.keys()

            if "From" in keys:
                email_from = spec_dict["From"]
            else:
                email_from = ""

            if "To" in keys:
                to = spec_dict["To"]
            else:
                to = ""

            if "Subject" in keys:
                subject = spec_dict["Subject"]
            else:
                subject = ""

            if "Username" in keys:
                usr_name = spec_dict["Username"]
            else:
                usr_name = ""

            if "Password" in keys:
                pwd = spec_dict["Password"]
            else:
                pwd = ""
        else:
            flag = 1
    else:
        flag = 1

    if flag == 1:
        email_from = ""
        to = ""
        subject = ""
        usr_name = ""
        pwd = ""

    values = (ctm, sip, sp, dip, dp, src_account, dst_account, type_str, email_from, to, subject, usr_name, pwd, details, attach_path, attachment)
    return values

def parse_tftp_file(dctn):

    flag = 0
    (ctm, sip, sp, dip, dp, src_account, dst_account, type_num, details, attach_path, attachment) = parse_common_info(dctn)
    details = ""
    type_dict = {"0":"none", "1":"PUT", "2":"GET"}
    try:
        type_str = type_dict[type_num]
    except KeyError:
        #print "tftp: error occour"
        type_str = type_num    
    
    if "fnm" in dctn.keys():
        spec_dict = {}
        dataname = os.path.basename(dctn["fnm"])
        if len(dataname):
            filepath = os.path.join(common.SHARE_PATH, dataname)
            ret = parse_file_to_dict(filepath, spec_dict)            
        else:
            ret = False
            #print dctn["fnm"], dataname          

        if ret:
            keys = spec_dict.keys()

            if "File-Name" in keys:
                filename = spec_dict["File-Name"]
            else:
                filename = ""

            if "Username" in keys:
                usr_name = spec_dict["Username"]
            else:
                usr_name = ""

            if "Password" in keys:
                pwd = spec_dict["Password"]
            else:
                pwd = ""

            if "cmd" in keys:
                cmd = spec_dict["cmd"]
            else:
                cmd = ""
                
            if "File-Path" in keys:
                file_path = spec_dict["File-Path"]
            else:
                file_path = ""
        else:
            flag = 1
    else:
        flag = 1

    if flag == 1:
        cmd = ""
        usr_name = ""
        pwd = ""
        file_path = ""
        filename = ""
    
    prefix = common.SERVERPATH_PREFIX
    protdir = dctn["protocol"]
    details = os.path.join(prefix, protdir, file_path, filename)
    
    values = (ctm, sip, sp, dip, dp, src_account, dst_account, usr_name, pwd, cmd, details)
    return values


def parse_im_file(dctn):

    flag = 0
    (ctm, sip, sp, dip, dp, type, src_account, dst_account, type_num, details, attach_path, attachment) = parse_common_info(dctn)

    keys = dctn.keys()

    if "gid" in keys:
        group_id = dctn["gid"]
    else:
        group_id = 0

    if "login_id" in keys:
        login_id = dctn["login_id"]
    else:
        login_id = 0

    values = (ctm, send_id, rec_id, sip, dip, sp, dp, src_account, dst_account, login_id, group_id, type_num, details)
    return values
