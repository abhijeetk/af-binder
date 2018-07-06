
.PHONY: all 

all: report

.PHONY: binaries

binaries: 
	make -C bin -w binaries

.PHONY: clean cleanall

clean:
	make -C bin -w clean

cleanall: clean
	rm -rf lcov-out.info report valgrind.out

.PHONY: test

test: binaries
	@echo -----------------------------------------
	@echo -- BEGIN TEST
	@echo -----------------------------------------
	@scripts/run-test.sh
	@echo -----------------------------------------
	@echo -- END TEST
	@echo -----------------------------------------

report: test
	@echo generating LCOV report
	@lcov -c -d bin -o lcov-out.info
	@echo generating report
	@genhtml -s -o report lcov-out.info
	@echo ready: xdg-open report/index.html
