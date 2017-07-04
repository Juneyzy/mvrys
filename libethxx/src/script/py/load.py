#!/usr/bin/python

import os
import sys
import string
import socket, time
#import cx_Oracle
#from oradbClass import OraDB
import parse_file
import common

def Usage():
    print "Usage: ", sys.argv[0], "index_filename"
    #parse_file.type_help()
    #print "TYPE: 1 - html"
    #print "      2 - email"
    #print "      3 - tftp"

def open_file(path_name):
    if not os.path.isfile(path_name):
        print  path_name, " Is not a file!"
        exit(-1)
    fp = open(path_name, 'r')
    return fp

def close_file(fd):
    fd.close()

def one_insert(table_name, val):

    sql_insert = "insert into {0:s} values {1:s}".format(table_name, str(val))
    #print sql_insert
    send_to_serv(sql_insert, False)
    #send_to_serv(sql_insert)
    return
"""
    db = OraDB()
    db.oracle_open()    
    try:
        db.insert_table_fullval(table_name, val)
        #db.show_all_table(table_name)
        ret = True
    except Exception as e:
        print "A error happen: ", e
        ret = False
    finally:
        db.oracle_close()
        return ret
"""
def send_to_serv(msg, remote=True):
    #addr = "/var/run/oracle.sock"
    if remote:
        cli_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        cli_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        host = "192.168.40.210"
        port = 44556

        totalen = len(msg)
        sendlen = 0
        try:
            cli_sock.connect((host, port))
            while sendlen < totalen:
                sendlen += cli_sock.send(msg[sendlen:])
                
            #buf = cli_sock.recv(32)
            #print buf
            #time.sleep(0.1)        
        except Exception as e:
            print "have a error:", e
        finally:                
            cli_sock.close()
    else:
        addr = "/var/run/oracl"
        if os.path.exists(addr):
            cli_sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            try:
                cli_sock.connect(addr)
            except Exception as e:
                err = str(e)
                cli_sock.close()
                if err.find("Permission denied"):
                    sys.stderr.write("use root user execute\r\n")
                else:
                    sys.stderr.write("{0}\r\n".format(err))
            cli_sock.send(msg)
            cli_sock.close()
        else:
            print "{0} not exists".format(addr)
        
       

def deal_entry(filename, once = 1, type = 1):

    try:
        fp = open_file(filename)
        contents = fp.readlines()
        close_file(fp)
    except IOError as e:
        err = str(e)
        if err.find("Too many open files"):
            print "Too many open files: failed open {0:s}".format(filename)
        return False
    
    common.SHARE_PATH = os.path.dirname(filename)

    dctn = {}
    ret = parse_file.parse_list_to_dict(contents, dctn)
    if ret == False:
        return ret
    
    if "protocol" not in dctn.keys() or len(dctn["protocol"]) == 0:
        print "protocol error"
        return False

    parserFile_func, table_name = parse_file.call_func(dctn["protocol"])
    
    if parserFile_func == None:
        return False         
        
    val = parserFile_func(dctn)
    if once == 1:
        return one_insert(table_name, val)
    else:
        return table_name, val

if __name__ == "__main__":
    #print sys.argv[0], sys.argv[1]#, sys.argv[2]
    #if len(sys.argv) != 2:
    #    Usage()
    #    exit(-1)

    #type = int(sys.argv[2])
    deal_entry(sys.argv[1])

else: 
    pass


