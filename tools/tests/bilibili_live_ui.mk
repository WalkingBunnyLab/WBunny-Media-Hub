# Test-only desktop binary; never linked into the Wii U application.
live-ui-preview: $(BUILD)/wbunny-live-preview
$(BUILD)/wbunny-live-preview: tools/tests/bilibili_live_ui_transport.cpp $(OBJS)
	$(CXX) $(CXXFLAGS) -Wl,--wrap=_ZN4http7performERKNS_7RequestE -o $@ $^ $(LDLIBS)
