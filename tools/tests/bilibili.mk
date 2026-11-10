# make -f desktop.mk -f tools/tests/bilibili.mk bilibili-tests
# build-desktop/bilibili-test [--live BV...]
bilibili-tests: $(BUILD)/bilibili-test
$(BUILD)/bilibili-test: tools/tests/bilibili_test.cpp $(filter-out $(BUILD)/src/main.o,$(OBJS))
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)
.PHONY: bilibili-tests
