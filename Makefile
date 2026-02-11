test:
	@command -v gcc >/dev/null 2>&1 || { \
		if echo "$$LANG" | grep -qi "pt_BR"; then \
			echo "Erro: gcc não está instalado. Instale o GCC antes de continuar."; \
		else \
			echo "Error: gcc is not installed. Please install GCC before continuing."; \
		fi; \
		exit 1; \
	}
	@mkdir -p build
	@gcc ./assertx.c -o ./assertx
	@./assertx ./tests
