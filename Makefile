DOCKERTAGSCLANG := lopoeisr/ubuntu-make-clang:1.0 lopoeisr/ubuntu-make-clang:latest
DOCKERTAGSGCC := lopoeisr/ubuntu-make-gcc:1.0 lopoeisr/ubuntu-make-gcc:latest
DOCKERTAGSDOC := lopoeisr/ubuntu-make-doc:1.0 lopoeisr/ubuntu-make-doc:latest
DOCKERTAGSNODE := lopoeisr/node-pkg:1.0 lopoeisr/node-pkg:latest

include .scaffold/master.mk

image:
	@docker build -f dockerfiles/clang.Dockerfile $(patsubst %, -t % ,$(DOCKERTAGSCLANG)) .
	@docker build -f dockerfiles/gcc.Dockerfile $(patsubst %, -t % ,$(DOCKERTAGSGCC)) .
	@docker build -f dockerfiles/documentation.Dockerfile $(patsubst %, -t % ,$(DOCKERTAGSDOC)) .
	@docker build -f dockerfiles/node.Dockerfile $(patsubst %, -t % ,$(DOCKERTAGSNODE)) .

image-push: image
	$(foreach tag, $(DOCKERTAGSCLANG), docker push $(tag); )
	$(foreach tag, $(DOCKERTAGSGCC), docker push $(tag); )
	$(foreach tag, $(DOCKERTAGSDOC), docker push $(tag); )
	$(foreach tag, $(DOCKERTAGSNODE), docker push $(tag); )
