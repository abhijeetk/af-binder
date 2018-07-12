
.PHONY: all 

all: report

.PHONY: binaries

binaries: 
	make -C bin -w binaries

.PHONY: clean cleanall

clean:
	make -C bin -w clean

cleanall: clean
	@echo remove all outputs and reports
	@rm -rf lcov-out.info report valgrind.out 2>/dev/null || true

.PHONY: test

test: binaries
	@echo -----------------------------------------
	@echo -- BEGIN TEST
	@echo -----------------------------------------
	@scripts/run-test.sh 2>&1 | tee run-test.output
	@echo -----------------------------------------
	@echo -- END TEST
	@echo -----------------------------------------

report: test
	@echo generating report
	@genhtml -s -o report lcov-out.info
	@echo ready: xdg-open report/index.html
