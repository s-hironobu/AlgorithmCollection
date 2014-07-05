MAKE = make --no-print-directory
DIRS = hash \
	list \
	queue

all:
	@for dir in $(DIRS) ; do \
		(cd $$dir && $(MAKE)) ; \
	done

test:
	@for dir in $(DIRS) ; do \
		(cd $$dir && $(MAKE) test) ; \
	done

clean:
	@for dir in $(DIRS) ; do \
		(cd $$dir && $(MAKE) clean) ; \
	done
