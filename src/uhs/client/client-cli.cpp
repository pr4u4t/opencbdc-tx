// Copyright (c) 2021 MIT Digital Currency Initiative,
//                    Federal Reserve Bank of Boston
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "commands.hpp"

#include <future>
#include <iostream>

// LCOV_EXCL_START
auto main(int argc, char** argv) -> int {
    static constexpr auto min_arg_count = 5;
    auto ret = 0;
    
    auto args = cbdc::config::get_args(argc, argv);
    auto parser = client::CommandParser();
    
    std::string usage = "Usage: " + args[0] + " <config file> <client file> <wallet file> <command>"
                      + " <args...>";
    
    if(args.size() < min_arg_count) {
        std::cerr << usage << std::endl;
        return 0;
    }

    auto cfg_or_err = cbdc::config::load_options(args[1]);
    if(std::holds_alternative<std::string>(cfg_or_err)) {
        std::cerr << "Error loading config file: "
                  << std::get<std::string>(cfg_or_err) << std::endl;
        return -1;
    }

    auto opts = std::get<cbdc::config::options>(cfg_or_err);

    SHA256AutoDetect();

    const auto wallet_file = args[3];
    const auto client_file = args[2];

    auto logger = std::make_shared<cbdc::logging::log>(
        cbdc::config::defaults::log_level);

    auto client = std::unique_ptr<cbdc::client>();
    
    if(opts.m_twophase_mode) {
        client = std::make_unique<cbdc::twophase_client>(opts,
                                                         logger,
                                                         wallet_file,
                                                         client_file);
    } else {
        client = std::make_unique<cbdc::atomizer_client>(opts,
                                                         logger,
                                                         wallet_file,
                                                         client_file);
    }

    if(!client->init()) {
        std::cerr << "failed to initialize client";
        return -2;
    }

    auto cargs = std::vector<std::string>(args.begin()+4,args.end());
    auto result = parser.execute(*client,cargs);
    
    switch(result.status()){
        case client::commands::CommandResult::Status::ERROR:
            std::cerr << result.errMsg();
            ret = result.code();
            break;
            
        case client::commands::CommandResult::Status::HELP:    
        case client::commands::CommandResult::Status::SUCCESS:
            std::cout << usage << std::endl;
            std::cout << static_cast<std::string>(result);
            break;
    }
    
    // TODO: check that the send queue has drained before closing
    //       the network. For now, just sleep.
    static constexpr auto shutdown_delay = std::chrono::milliseconds(100);
    std::this_thread::sleep_for(shutdown_delay);

    return ret;
}
// LCOV_EXCL_STOP
