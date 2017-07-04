APP = app
APP_NEW = app_new

$(APP):
	cc test.c -o $@ -I include -L./lib -lethxx -lpthread -lpcre -lpcap -lyaml 

$(APP_NEW):
	cc test_new.c -o $@ -I include -L./lib -lethxx -lpthread -lpcre -lpcap -lyaml 
clean:
	@rm  $(APP) $(APP_NEW) #*.d *.o 	
