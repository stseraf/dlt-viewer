#include <gtest/gtest.h>

#include <qdltctrlmsg.h>

#include <QByteArray>

TEST(CtrlPayload, parse) {
    // this is real payload of a dlt control message which collect from dlt-daemon
    const auto data = QByteArray::fromHex("030000000701005359530001004d475200ffff2200436f6e74657874206f66206d61696e20646c742073797374656d206d616e616765721200444c542053797374656d204d616e6167657272656d6f");

    auto payload = qdlt::msg::payload::parse(data, false);

    ASSERT_TRUE(std::holds_alternative<qdlt::msg::payload::GetLogInfo>(payload));
    auto getLogInfo = std::get<qdlt::msg::payload::GetLogInfo>(payload);
    EXPECT_EQ(getLogInfo.status, 7);
    ASSERT_EQ(getLogInfo.apps.size(), 1);

    auto app = getLogInfo.apps[0];
    EXPECT_EQ(app.id, "SYS");
    EXPECT_EQ(app.description, "DLT System Manager");
    ASSERT_EQ(app.ctxs.size(), 1);

    auto ctx = app.ctxs[0];
    EXPECT_EQ(ctx.id, "MGR");
    EXPECT_EQ(ctx.logLevel, '\xff');
    EXPECT_EQ(ctx.traceStatus, '\xff');
    EXPECT_EQ(ctx.description, "Context of main dlt system manager");
}

TEST(CtrlPayload, parseStatus6HasNoDescriptions) {
    // Status 6 response: application and context ids with log level and trace
    // status, but without any description. Two contexts, so that a parser which
    // wrongly expects a description would desync on the second one.
    const auto data = QByteArray::fromHex("030000000601005359530002004d4752000403"
                                          "4d4752320102");

    auto payload = qdlt::msg::payload::parseAutoDetect(data, false);

    ASSERT_TRUE(std::holds_alternative<qdlt::msg::payload::GetLogInfo>(payload));
    auto getLogInfo = std::get<qdlt::msg::payload::GetLogInfo>(payload);
    EXPECT_EQ(getLogInfo.status, 6);
    EXPECT_TRUE(getLogInfo.hasLogLevel);
    EXPECT_TRUE(getLogInfo.hasTraceStatus);
    EXPECT_FALSE(getLogInfo.hasDescription);
    ASSERT_EQ(getLogInfo.apps.size(), 1);

    auto app = getLogInfo.apps[0];
    EXPECT_EQ(app.id, "SYS");
    EXPECT_EQ(app.description, "");
    ASSERT_EQ(app.ctxs.size(), 2);

    EXPECT_EQ(app.ctxs[0].id, "MGR");
    EXPECT_EQ(app.ctxs[0].logLevel, 4);
    EXPECT_EQ(app.ctxs[0].traceStatus, 3);
    EXPECT_EQ(app.ctxs[0].description, "");

    EXPECT_EQ(app.ctxs[1].id, "MGR2");
    EXPECT_EQ(app.ctxs[1].logLevel, 1);
    EXPECT_EQ(app.ctxs[1].traceStatus, 2);
}

TEST(CtrlPayload, parseStatus4HasLogLevelOnly) {
    // Status 4 response: ids with log level, no trace status and no description.
    const auto data = QByteArray::fromHex("030000000401005359530001004d47520005");

    auto payload = qdlt::msg::payload::parseAutoDetect(data, false);

    ASSERT_TRUE(std::holds_alternative<qdlt::msg::payload::GetLogInfo>(payload));
    auto getLogInfo = std::get<qdlt::msg::payload::GetLogInfo>(payload);
    EXPECT_EQ(getLogInfo.status, 4);
    EXPECT_TRUE(getLogInfo.hasLogLevel);
    EXPECT_FALSE(getLogInfo.hasTraceStatus);
    EXPECT_FALSE(getLogInfo.hasDescription);
    ASSERT_EQ(getLogInfo.apps.size(), 1);
    ASSERT_EQ(getLogInfo.apps[0].ctxs.size(), 1);
    EXPECT_EQ(getLogInfo.apps[0].ctxs[0].id, "MGR");
    EXPECT_EQ(getLogInfo.apps[0].ctxs[0].logLevel, 5);
}

TEST(CtrlPayload, parseUint8DescriptionLength) {
    // Same response as above but with AUTOSAR/Vector uint8 (1-byte) description
    // lengths (0x22 and 0x12) instead of the COVESA uint16 (0x2200 / 0x1200).
    const auto data = QByteArray::fromHex("030000000701005359530001004d475200ffff22436f6e74657874206f66206d61696e20646c742073797374656d206d616e6167657212444c542053797374656d204d616e6167657272656d6f");

    auto payload = qdlt::msg::payload::parse(data, false,
                                             qdlt::msg::payload::DescriptionLengthWidth::Uint8);

    ASSERT_TRUE(std::holds_alternative<qdlt::msg::payload::GetLogInfo>(payload));
    auto getLogInfo = std::get<qdlt::msg::payload::GetLogInfo>(payload);
    EXPECT_EQ(getLogInfo.status, 7);
    ASSERT_EQ(getLogInfo.apps.size(), 1);

    auto app = getLogInfo.apps[0];
    EXPECT_EQ(app.id, "SYS");
    EXPECT_EQ(app.description, "DLT System Manager");
    ASSERT_EQ(app.ctxs.size(), 1);

    auto ctx = app.ctxs[0];
    EXPECT_EQ(ctx.id, "MGR");
    EXPECT_EQ(ctx.description, "Context of main dlt system manager");
}
