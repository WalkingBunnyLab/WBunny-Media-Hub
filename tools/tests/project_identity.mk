# make -f desktop.mk -f tools/tests/project_identity.mk project-identity-tests
project-identity-tests: $(BUILD)/project-identity-test
$(BUILD)/project-identity-test: tools/tests/project_identity_test.cpp $(filter-out $(BUILD)/src/main.o,$(OBJS))
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)
.PHONY: project-identity-tests
