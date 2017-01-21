/*
 * Copyright 2016 Quora, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <tuple>
#include <vector>


const auto KB = 1024lu;
const auto MB = 1024lu * KB;
const auto GB = 1024lu * MB;


typedef std::vector<std::tuple<const char *, const char *, const char *>> Args;
const char *is_arg(const char *arg, const char *arg_key);

