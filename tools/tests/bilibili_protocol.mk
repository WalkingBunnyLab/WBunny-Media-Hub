# Standalone offline test: C++20 compiler, pkg-config, SDL2 headers, Jansson and OpenSSL.
# make -f tools/tests/bilibili_protocol.mk test
# HTTP is mocked; production service, URL encoding and MD5 are linked unchanged.
CXX ?= c++
LIVE_SOURCES := src/services/bilibili_live.cpp src/core/util.cpp src/core/md5.cpp
PROBE_BUILD := build/bilibili-protocol
PROBE_CFLAGS := -std=gnu++20 -O1 -g -Wall -Werror -Wno-sign-compare \
                -ffunction-sections -fdata-sections -DBILIBILI_TEST_TRANSPORT -Isrc \
                $(shell pkg-config --cflags sdl2 jansson openssl)
ifeq ($(shell uname -s),Darwin)
PROBE_GC := -Wl,-dead_strip
else
PROBE_GC := -Wl,--gc-sections
endif

$(PROBE_BUILD)/test: tools/tests/bilibili_test.cpp src/services/bilibili.cpp src/core/util.cpp src/core/md5.cpp
	@mkdir -p $(PROBE_BUILD)
	$(CXX) $(PROBE_CFLAGS) $^ $(PROBE_GC) $(shell pkg-config --libs jansson openssl) -pthread -o $@

test: $(PROBE_BUILD)/test
	./$(PROBE_BUILD)/test

# Optional read-only network check: ./build/bilibili-protocol/live --live BV...
live: $(PROBE_BUILD)/live
$(PROBE_BUILD)/live: tools/tests/bilibili_test.cpp src/services/bilibili.cpp src/core/util.cpp src/core/md5.cpp src/core/http.cpp tools/tests/bilibili_live_support.cpp
	@mkdir -p $(PROBE_BUILD)
	$(CXX) $(filter-out -DBILIBILI_TEST_TRANSPORT,$(PROBE_CFLAGS)) $(shell pkg-config --cflags libcurl) $^ $(PROBE_GC) $(shell pkg-config --libs jansson openssl libcurl) -pthread -o $@
.PHONY: test live

recommend-test: $(PROBE_BUILD)/recommend-test
	./$(PROBE_BUILD)/recommend-test
$(PROBE_BUILD)/recommend-test: tools/tests/bilibili_recommend_test.cpp src/services/bilibili.cpp src/core/util.cpp src/core/md5.cpp
	@mkdir -p $(PROBE_BUILD)
	$(CXX) $(PROBE_CFLAGS) $^ $(PROBE_GC) $(shell pkg-config --libs jansson openssl) -pthread -o $@

account-test: $(PROBE_BUILD)/account-test
	./$(PROBE_BUILD)/account-test
$(PROBE_BUILD)/account-test: tools/tests/bilibili_account_test.cpp src/services/bilibili_account.cpp src/services/bilibili.cpp src/core/util.cpp src/core/md5.cpp
	@mkdir -p $(PROBE_BUILD)
	$(CXX) $(PROBE_CFLAGS) $^ $(PROBE_GC) $(shell pkg-config --libs jansson openssl) -pthread -o $@

account-live: $(PROBE_BUILD)/account-live
$(PROBE_BUILD)/account-live: tools/tests/bilibili_account_live.cpp src/services/bilibili_account.cpp src/services/bilibili.cpp src/core/util.cpp src/core/md5.cpp src/core/http.cpp tools/tests/bilibili_live_support.cpp
	@mkdir -p $(PROBE_BUILD)
	$(CXX) $(filter-out -DBILIBILI_TEST_TRANSPORT,$(PROBE_CFLAGS)) $(shell pkg-config --cflags libcurl) $^ $(PROBE_GC) $(shell pkg-config --libs jansson openssl libcurl) -pthread -o $@

live-test: $(PROBE_BUILD)/live-test
	./$(PROBE_BUILD)/live-test
$(PROBE_BUILD)/live-test: tools/tests/bilibili_live_test.cpp $(LIVE_SOURCES) tools/tests/bilibili_live_support.cpp
	@mkdir -p $(PROBE_BUILD)
	$(CXX) $(PROBE_CFLAGS) -DBILIBILI_LIVE_TEST_CLIENT $^ $(PROBE_GC) $(shell pkg-config --libs jansson openssl) -pthread -o $@

live-probe: $(PROBE_BUILD)/live-probe
	./$(PROBE_BUILD)/live-probe
$(PROBE_BUILD)/live-probe: tools/tests/bilibili_live_probe.cpp $(LIVE_SOURCES) src/core/http.cpp tools/tests/bilibili_live_support.cpp build/bilibili_live_client.hpp
	@mkdir -p $(PROBE_BUILD)
	$(CXX) $(filter-out -DBILIBILI_TEST_TRANSPORT,$(PROBE_CFLAGS)) -Ibuild $(shell pkg-config --cflags libcurl) $(filter %.cpp,$^) $(PROBE_GC) $(shell pkg-config --libs jansson openssl libcurl) -pthread -o $@
