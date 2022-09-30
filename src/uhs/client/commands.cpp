#include "commands.hpp" 

#define HELP_CHECK ( args[1] == "h" || args[1] == "-h" || args[1] == "help" || args[1] == "--help")

namespace client {
    
    CommandParser::CommandParser(){
        register_command("mint",std::function<commands::Command>(commands::mint));
        register_command("send",std::function<commands::Command>(commands::send));
        register_command("fan",std::function<commands::Command>(commands::fan));
        register_command("new_address",std::function<commands::Command>(commands::newaddress));
        register_command("importinput",std::function<commands::Command>(commands::importinput));
        register_command("confirmtx",std::function<commands::Command>(commands::confirmtx));
        register_command("info",std::function<commands::Command>(commands::info));
        register_command("sync",std::function<commands::Command>(commands::info));
        register_command("help",std::function<commands::Command>(m_help));
    }
    
    auto CommandParser::execute(cbdc::client &client,const std::vector<std::string>& cmd) -> commands::CommandResult{
        if (m_commands.find(cmd[0]) == m_commands.end()){
            return commands::CommandResult(commands::CommandResult::Status::ERROR, "Unknown command: " + cmd[0] + "\n");
        }

        return m_commands[cmd[0]](client,cmd);
    }
    
    void CommandParser::register_command(const std::string& name, 
                                         std::function<commands::Command>&& hwnd){
         m_commands[name] = hwnd;                                    
    }
    
    namespace commands {
        
        template<>
        [[nodiscard]] auto CommandResult::TypedData::get() const -> std::optional<int>{
            return (type() == INT) ? std::optional<int>(std::get<int>(d)) : std::nullopt;
        }
        
        template<>
        [[nodiscard]] auto CommandResult::TypedData::get() const -> std::optional<unsigned int>{
            return (type() == UINT) ? std::optional<int>(std::get<unsigned int>(d)) : std::nullopt;
        }
        
        template<>
        [[nodiscard]] auto CommandResult::TypedData::get() const -> std::optional<long>{
            return (type() == LONG) ? std::optional<long>(std::get<long>(d)) : std::nullopt;
        }
        
        template<>
        [[nodiscard]] auto CommandResult::TypedData::get() const -> std::optional<unsigned long>{
            return (type() == ULONG) ? std::optional<long>(std::get<unsigned long>(d)) : std::nullopt;
        }
        
        template<>
        [[nodiscard]] auto CommandResult::TypedData::get() const -> std::optional<float>{
            return (type() == FLOAT) ? std::optional<float>(std::get<float>(d)) : std::nullopt;
        }
        
        template<>
        [[nodiscard]] auto CommandResult::TypedData::get() const -> std::optional<double>{
            return (type() == DOUBLE) ? std::optional<double>(std::get<double>(d)) : std::nullopt;
        }
        
        template<>
        auto CommandResult::TypedData::get() const -> std::optional<std::string>{
            return (type() == STRING) ? std::optional<std::string>(std::get<std::string>(d)) : std::nullopt;
        }
        
        template<>
        auto CommandResult::TypedData::get() const -> std::optional<std::vector<TypedData>>{
            return (type() == ARRAY) ? std::optional<std::vector<TypedData>>(std::get<std::vector<TypedData>>(d)) 
                                     : std::nullopt;
        }
        
        template<>
        auto CommandResult::TypedData::get() const -> std::optional<std::unordered_map<std::string,TypedData>>{
            return (type() == MAP) ? std::optional<std::unordered_map<std::string,TypedData>>(std::get<std::unordered_map<std::string,TypedData>>(d))
                                   : std::nullopt;
        }
        
        CommandResult::CommandResult(CommandResult::Status status, std::string err,
                                    const std::function<std::string(const CommandResult::TypedData&)> &f) :
                                    m_status(status),m_err(std::move(err)),m_data(std::unordered_map<std::string,TypedData>()),
                                    m_code(0){
            
            m_formatter = f ? f : [&](const CommandResult::TypedData& m){
                std::ostringstream os;

                switch(m.type()){
                    case CommandResult::Type::ARRAY:{
                        int i = 0;
                        std::vector<TypedData> vec = m.get<std::vector<TypedData>>().value();
                        for (const CommandResult::TypedData &mn : vec){
                            os << i << " " << m_formatter(mn);
                        }
                        break;
                    }
                        
                    case CommandResult::Type::MAP:{
                        std::unordered_map<std::string,TypedData> map = m.get<std::unordered_map<std::string,TypedData>>().value();
                        for (std::pair<const std::string, CommandResult::TypedData>& mn : map) {
                            os << mn.first << " " << m_formatter(mn.second) << std::endl;
                        }
                        break;
                    }
                    case CommandResult::Type::INT:
                        os << m.get<int>().value_or(0);
                        break;
                            
                    case CommandResult::Type::UINT:
                        os << m.get<unsigned int>().value_or(0);
                        break;
                            
                    case CommandResult::Type::LONG:
                        os << m.get<long>().value_or(0);
                        break;
                            
                    case CommandResult::Type::ULONG:
                        os << m.get<unsigned long>().value_or(0);
                        break;
                            
                    case CommandResult::Type::FLOAT:
                        os << m.get<float>().value_or(0);
                        break;
                            
                    case CommandResult::Type::STRING:
                        os << m.get<std::string>().value_or("");
                        break;

                    case CommandResult::Type::DOUBLE:
                        os << m.get<double>().value_or(0.0);
                        break;
                        
                    default:
                        os << "Unknown data";
                        break;
                }

                return os.str();
            };
        }
        
        auto mint(cbdc::client& client, const std::vector<std::string>& args)
        -> commands::CommandResult {
            static constexpr auto min_mint_arg_count = 3;
            static constexpr auto n_output_idx = 1;
            static constexpr auto output_val_idx = 2;
            
            if(args.size() < min_mint_arg_count || HELP_CHECK) {
                return commands::CommandResult(CommandResult::Status::ERROR,"<n outputs> <output value>");
            }
            
            const auto n_outputs = std::stoull(args[n_output_idx]);
            const auto output_val = std::stoul(args[output_val_idx]);
            
            const auto mint_tx
            = client.mint(n_outputs, static_cast<uint32_t>(output_val));
            
            CommandResult result(CommandResult::Status::SUCCESS );
            result.set("Transaction ID",cbdc::to_string(cbdc::transaction::tx_id(mint_tx)));
            
            return result;
        }
        
        void print_tx_result(
            const std::optional<cbdc::transaction::full_tx>& tx,
            const std::optional<cbdc::sentinel::execute_response>& resp,
            const cbdc::hash_t& pubkey,CommandResult& res) {
            
            res.set("tx_id",cbdc::to_string(cbdc::transaction::tx_id(tx.value())));
            
            const auto inputs = cbdc::client::export_send_inputs(tx.value(), pubkey);
            std::vector<CommandResult::TypedData> v;
            for(const auto& inp : inputs) {
                auto buf = cbdc::make_buffer(inp);
                v.emplace_back(buf.to_hex());
            }
            
            res.set("Data for recipient importinput", v);
            
            if(resp.has_value()) {
                res.set("Sentinel responded",cbdc::sentinel::to_string(resp.value().m_tx_status));
                if(resp.value().m_tx_error.has_value()) {
                    res.setErrMsg("Validation error: "+cbdc::transaction::validation::to_string(
                        resp.value().m_tx_error.value()));
                }
            }
        }
        
        auto send(cbdc::client& client, const std::vector<std::string>& args)
            -> CommandResult {
            static constexpr auto min_send_arg_count = 3;
            static constexpr auto value_arg_idx = 1;
            static constexpr auto address_arg_idx = 2;
            
            if(args.size() < min_send_arg_count || HELP_CHECK) {
                return commands::CommandResult(CommandResult::Status::ERROR , "<value> <pubkey>");
            }
            
            const auto value = std::stoul(args[value_arg_idx]);
            
            auto pubkey = cbdc::address::decode(args[address_arg_idx]);
            if(!pubkey.has_value()) {
                return commands::CommandResult(CommandResult::Status::ERROR, "send: Could not decode address" );
            }
            
            const auto [tx, resp]
            = client.send(static_cast<uint32_t>(value), pubkey.value());
            if(!tx.has_value()) {
                return commands::CommandResult(CommandResult::Status::ERROR, "send: Could not generate valid send tx");
            }
            
            CommandResult result(CommandResult::Status::SUCCESS);
            print_tx_result(tx, resp, pubkey.value(),result);
            
            return result;
        }
        
        auto fan(cbdc::client& client, const std::vector<std::string>& args)
            -> CommandResult {
            
            static constexpr auto count_arg_idx = 1;
            static constexpr auto value_arg_idx = 2;
            static constexpr auto address_arg_idx = 3;
            static constexpr auto min_fan_arg_count = 4;
            
            if(args.size() < min_fan_arg_count || HELP_CHECK) {
                return commands::CommandResult(CommandResult::Status::ERROR, "<count> <value> <pubkey>");
            }
            
            const auto value = std::stoul(args[value_arg_idx]);
            const auto count = std::stoul(args[count_arg_idx]);
            auto pubkey = cbdc::address::decode(args[address_arg_idx]);
            
            if(!pubkey.has_value()) {
                return commands::CommandResult(CommandResult::Status::ERROR, "fan: Could not decode address");
            }
            
            const auto [tx, resp] = client.fan(static_cast<uint32_t>(count),
                                               static_cast<uint32_t>(value),
                                               pubkey.value());
            if(!tx.has_value()) {
                return commands::CommandResult(CommandResult::Status::ERROR, "fan: Could not generate valid send tx");
            }
            
            CommandResult result(CommandResult::Status::SUCCESS);
            print_tx_result(tx, resp, pubkey.value(),result);
            return result;
        }
        
        auto newaddress(cbdc::client& client,const std::vector<std::string>& args) -> CommandResult{
            
            if(HELP_CHECK){
                return commands::CommandResult(CommandResult::Status::ERROR);
            }
            
            const auto addr = client.new_address();
            auto addr_vec
            = std::vector<uint8_t>(sizeof(cbdc::client::address_type::public_key)
            + std::tuple_size<decltype(addr)>::value);
            
            addr_vec[0] = static_cast<uint8_t>(cbdc::client::address_type::public_key);
            std::copy_n(addr.begin(),
                        addr.size(),
                        addr_vec.begin()
                        + sizeof(cbdc::client::address_type::public_key));
            
            auto data = std::vector<uint8_t>();
            CommandResult result(CommandResult::Status::SUCCESS); 
            ConvertBits<cbdc::address::bits_per_byte, cbdc::address::bech32_bits_per_symbol, true>(
                [&](uint8_t c) {
                    data.push_back(c);
                },
                addr_vec.begin(),
                addr_vec.end());
            
            result.set("Address",bech32::Encode(cbdc::config::bech32_hrp, data));
            
            return result;
        }
        
        auto importinput(cbdc::client& client,
                                 const std::vector<std::string>& args) -> CommandResult {
            static constexpr auto input_arg_idx = 1;
            static constexpr auto min_import_arg_count = 2;
            
            if(args.size() < min_import_arg_count || HELP_CHECK) {
                return commands::CommandResult(CommandResult::Status::ERROR, "<input>");
            }
            
            auto buffer = cbdc::buffer::from_hex(args[input_arg_idx]);
            if(!buffer.has_value()) {
                return commands::CommandResult(CommandResult::Status::ERROR, "importinput: Invalid input encoding");
            }
                                     
            auto in = cbdc::from_buffer<cbdc::transaction::input>(buffer.value());
            if(!in.has_value()) {
                return commands::CommandResult(CommandResult::Status::ERROR, "importinput: Invalid input");
            }
            client.import_send_input(in.value());
            return commands::CommandResult(CommandResult::Status::SUCCESS);
        }
        
        auto confirmtx(cbdc::client& client,
                               const std::vector<std::string>& args) -> CommandResult {
            static constexpr auto tx_id_arg_idx = 1;
            static constexpr auto confirmtx_min_arg_count = 2;
            
            if(args.size() < confirmtx_min_arg_count || HELP_CHECK) {
                return commands::CommandResult(CommandResult::Status::ERROR, "<tx_id>");
            }
            
            const auto tx_id = cbdc::hash_from_hex(args[tx_id_arg_idx]);
            auto success = client.confirm_transaction(tx_id);
            if(!success) {
                return commands::CommandResult(CommandResult::Status::ERROR, "confirmtx: Unknown TXID");
            }
            
            CommandResult result(CommandResult::Status::SUCCESS);
            result.set("Confirmed Balance",cbdc::client::print_amount(client.balance()));
            result.set("UTXOs",client.utxo_count());
                      
            return commands::CommandResult(CommandResult::Status::SUCCESS);
        }
        
        auto info(cbdc::client& client,
                  const std::vector<std::string>& args) -> CommandResult{
            if(HELP_CHECK){
                return commands::CommandResult(CommandResult::Status::ERROR);
            }
            
            const auto balance = client.balance();
            const auto n_txos = client.utxo_count();
            CommandResult result(CommandResult::Status::SUCCESS);
            result.set("balance", cbdc::client::print_amount(balance));
            result.set("UTXOs", n_txos);
            result.set("pending TXs",client.pending_tx_count());
            
            return result;
        }
        
        auto sync(cbdc::client& client,const std::vector<std::string>& args) -> commands::CommandResult{
            if(HELP_CHECK){
                return commands::CommandResult(CommandResult::Status::ERROR);
            }
            
            client.sync();
            
            return commands::CommandResult(CommandResult::Status::SUCCESS);
        }
    }
}
