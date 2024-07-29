DOCKERTAGSCLANG := lopoeisr/ubuntu-make-clang:18.1 lopoeisr/ubuntu-make-clang:latest
DOCKERTAGSNODE := lopoeisr/node-pkg:18 lopoeisr/node-pkg:latest

include master.mk

image:
	@docker build -f dockerfiles/clang.Dockerfile $(patsubst %, -t % ,$(DOCKERTAGSCLANG)) .
	@docker build -f dockerfiles/node.Dockerfile $(patsubst %, -t % ,$(DOCKERTAGSNODE)) .

image-push: image
	$(foreach tag, $(DOCKERTAGSCLANG), docker push $(tag); )
	$(foreach tag, $(DOCKERTAGSNODE), docker push $(tag); )
