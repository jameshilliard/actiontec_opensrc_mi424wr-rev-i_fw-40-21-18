ARCHCONFIG_FIRST_TASKS+=create_modules_h

create_modules_h:
	$(foreach module, $(AST_MODULES_NO_RELOAD), echo MODULE_FUNCTIONS_NO_RELOAD\($(module)\) >> ../prototypes.h; )
	$(foreach module, $(AST_MODULES_WITH_RELOAD), echo MODULE_FUNCTIONS_WITH_RELOAD\($(module)\) >> ../prototypes.h; )
	$(foreach module, $(AST_MODULES_NO_RELOAD), echo M_NO_RELOAD\($(module)\), >> ../ast_modules.h; )
	$(foreach module, $(AST_MODULES_WITH_RELOAD), echo M_WITH_RELOAD\($(module)\), >> ../ast_modules.h; )

COMMAND_FILE=variable.mak
FOR_EACH=$(O_OBJS)
INDEX_VARIABLE=CFLAGS_$(INDEX)
INDEX_VAL=-Dload_module=$(patsubst %.o,%,$(INDEX))_load_module \
  -Dunload_module=$(patsubst %.o,%,$(INDEX))_unload_module \
  -Dreload=$(patsubst %.o,%,$(INDEX))_reload \
  -Ddescription=$(patsubst %.o,%,$(INDEX))_description \
  -Dkey=$(patsubst %.o,%,$(INDEX))_key \
  -Dusecount=$(patsubst %.o,%,$(INDEX))_usecount
include $(LOOP)

