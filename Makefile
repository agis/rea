VERSION=0.0.1

rea: http_parser.c rea.c
	gcc -Wall http_parser.c rea.c -o rea

clean:
	@rm -f rea

docker-image: Dockerfile rea
	docker build -t rea:$(VERSION) .

docker-run: docker-image
	docker run --rm -p 8080:80 rea:$(VERSION)
