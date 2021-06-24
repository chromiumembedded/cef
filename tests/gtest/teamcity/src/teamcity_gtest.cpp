/* Copyright 2015 Paul Shmakov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "tests/gtest/teamcity/include/teamcity_gtest.h"

namespace jetbrains {
namespace teamcity {

using namespace testing;

TeamcityGoogleTestEventListener::TeamcityGoogleTestEventListener() {
    flowid = getFlowIdFromEnvironment();
}

TeamcityGoogleTestEventListener::TeamcityGoogleTestEventListener(const std::string& flowid_)
    : flowid(flowid_) {
}

// Fired before the test case starts.
void TeamcityGoogleTestEventListener::OnTestCaseStart(const TestCase& test_case) {
    messages.suiteStarted(test_case.name(), flowid);
}

// Fired before the test starts.
void TeamcityGoogleTestEventListener::OnTestStart(const TestInfo& test_info) {
    messages.testStarted(test_info.name(), flowid);
}

// Fired after the test ends.
void TeamcityGoogleTestEventListener::OnTestEnd(const TestInfo& test_info) {
    const TestResult* result = test_info.result();
    if (result->Failed()) {
        std::string message;
        std::string details;
        for (int i = 0; i < result->total_part_count(); ++i) {
            const TestPartResult& partResult = result->GetTestPartResult(i);
            if (partResult.passed()) {
                continue;
            }

            if (message.empty()) {
                message = partResult.summary();
            }

            if (!details.empty()) {
                details.append("\n");
            }
            details.append(partResult.message());

            if (partResult.file_name() && partResult.line_number() >= 0) {
                std::stringstream ss;
                ss << "\n at " << partResult.file_name() << ":" << partResult.line_number();
                details.append(ss.str());
            }
        }

        messages.testFailed(
            test_info.name(),
            !message.empty() ? message : "failed",
            details,
            flowid
        );
    }
    messages.testFinished(test_info.name(), static_cast<int>(result->elapsed_time()), flowid);
}

// Fired after the test case ends.
void TeamcityGoogleTestEventListener::OnTestCaseEnd(const TestCase& test_case) {
    messages.suiteFinished(test_case.name(), flowid);
}

}
}
