help:
	@echo Pro preklad bez vypisu stavu promennych zadejte:
	@echo make release
	@echo (Vytvori se program .\preteceni)
	@echo 
	@echo Pro preklad s vypisem stavu promennych zadejte:
	@echo make debug
	@echo (Vytvori se program .\preteceni_debug)
release:
	gcc preteceni.c -o preteceni -fno-stack-protector
debug:
	gcc preteceni.c -o preteceni_debug -fno-stack-protector -DKONTROLA
