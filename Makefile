DOCKERTAGSCLANG := lopoeisr/ubuntu-make-clang:18.2 lopoeisr/ubuntu-make-clang:latest
DOCKERTAGSGCC := lopoeisr/ubuntu-make-gcc:18.3 lopoeisr/ubuntu-make-gcc:latest
DOCKERTAGSNODE := lopoeisr/node-pkg:18.2 lopoeisr/node-pkg:latest

include .scaffold/master.mk

image:
	@docker build -f dockerfiles/clang.Dockerfile $(patsubst %, -t % ,$(DOCKERTAGSCLANG)) .
	@docker build -f dockerfiles/gcc.Dockerfile $(patsubst %, -t % ,$(DOCKERTAGSGCC)) .
	@docker build -f dockerfiles/node.Dockerfile $(patsubst %, -t % ,$(DOCKERTAGSNODE)) .

image-push: image
	$(foreach tag, $(DOCKERTAGSCLANG), docker push $(tag); )
	$(foreach tag, $(DOCKERTAGSGCC), docker push $(tag); )
	$(foreach tag, $(DOCKERTAGSNODE), docker push $(tag); )
