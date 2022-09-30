#ifndef OPENCBDC_TX_SRC_COMMANDS_CLIENT_H_
#define OPENCBDC_TX_SRC_COMMANDS_CLIENT_H_ 

#include <sstream>
#include <functional>
#include <optional>

#include "atomizer_client.hpp"
#include "client.hpp"
#include "crypto/sha256.h"
#include "twophase_client.hpp"
#include "util/common/config.hpp"

#include "util/serialization/util.hpp"
#include "uhs/transaction/messages.hpp"
#include "bech32/bech32.h"
#include "bech32/util/strencodings.h"

#define UNUSED(arg) ((void) arg)

namespace client {
    namespace commands {
        
        class CommandResult {
            public:
                struct TypedData;
                
                enum Type {
                    INVALID = -1,
                    INT = 0,
                    UINT,
                    LONG,
                    ULONG,
                    FLOAT,
                    DOUBLE,
                    STRING,
                    ARRAY,
                    MAP
                };
                
                using U = std::variant<int,
                                       unsigned int,
                                       long,
                                       unsigned long,
                                       float,
                                       double,
                                       std::string,
                                       std::vector<TypedData>,
                                       std::unordered_map<std::string,TypedData>>;
                                       
                struct TypedData{
                    public:
                        TypedData(const TypedData &other) = default;
                        
                        TypedData(TypedData&& other) noexcept : d(std::move(other.d)){}
                        
                        ~TypedData() = default;
                        
                        TypedData() = default;
                        
                        [[nodiscard]] auto type() const -> Type{
                            return static_cast<Type>(d.index());
                        }
                        
                        [[nodiscard]] auto typeAt(int i) const -> Type{
                            return (d.index() != Type::ARRAY) ? Type::INVALID 
                                    : static_cast<Type>(std::get<std::vector<TypedData>>(d)
                                    .at(static_cast<std::vector<TypedData>::size_type>(i)).type());
                        }
                        
                        [[nodiscard]] auto typeAt(const std::string& key) const -> Type{
                            return (d.index() != Type::MAP) ? Type::INVALID 
                            : static_cast<Type>(std::get<std::unordered_map<std::string,TypedData>>(d)
                            .at(key).type());                            
                        }
                        
                        template<typename T>
                        explicit TypedData(T t) : d(std::move(t)){}
                       
                        auto operator=(const TypedData& other) noexcept -> TypedData& = default;
                        
                        auto operator=(TypedData&& other) noexcept -> TypedData&{
                            d = other.d;
                            return *this;
                        }
                        
                        template<typename T>
                        auto set(T t) -> bool{
                            d = t;
                            return true;
                        }
                        
                        template<typename T>
                        auto set(int i,T t) -> bool{
                            if(d.index() != Type::ARRAY){
                                return false;
                            }
                            
                            if(std::get<std::vector<TypedData>>(d).size() <= static_cast<std::vector<TypedData>::size_type>(i)){
                                return false;
                            }
                            
                            std::get<std::vector<TypedData>>(d).at(i) = TypedData(t);
                            
                            return true;
                        }
                        
                        template<typename T>
                        auto set(const std::string& key,T t) -> bool{
                            if(d.index() != Type::MAP){
                                return false;
                            }
                            
                            std::get<std::unordered_map<std::string,TypedData>>(d).insert_or_assign(key,TypedData(t));
                            
                            return true;
                        }
                    
                        template<typename T>
                        auto get() -> std::optional<T> {
                            return {};
                        }
                        
                        template<typename T>
                        [[nodiscard]] auto get() const -> std::optional<T> {
                            return {};
                        }
                        
                    private:
                        U d;
                };
                
                enum Status {
                    SUCCESS,
                    ERROR,
                    HELP
                };
                
                explicit CommandResult(CommandResult::Status status, std::string err = "",
                                       const std::function<std::string(const CommandResult::TypedData&)> &f = nullptr);
                
                template<typename T>
                auto set(const std::string& key,T t) -> bool{
                    return m_data.set(key,t);                    
                }
                
                [[nodiscard]]auto valid() const -> bool{
                    return m_status == Status::SUCCESS;
                }
                
                explicit operator std::string() const{
                    std::string ret = (!m_err.empty()) ? m_err : std::string();
                    ret += (m_formatter) ?  m_formatter(m_data) : std::string();
                    return ret;
                }
                
                void setStatus(Status status){
                    m_status = status;
                }
                
                [[nodiscard]] auto status() const -> Status{
                    return m_status;
                }
                
                void setErrMsg(std::string err){
                    m_err = std::move(err);
                }
                
                [[nodiscard]] auto errMsg() const -> std::string{
                    return m_err;
                }
                
                void setCode(int code){
                    m_code = code;
                }
                
                [[nodiscard]] auto code() const -> int{
                    return m_code;
                }
                
            private:
                Status m_status;
                std::string m_err;
                TypedData m_data;
                int m_code;
                std::function<std::string(const CommandResult::TypedData&)> m_formatter;
        };
        
        using Command = CommandResult(cbdc::client&,const std::vector<std::string>&); 
        
        auto mint(cbdc::client& client, const std::vector<std::string>& args)
            -> commands::CommandResult; 
        
        auto send(cbdc::client& client, const std::vector<std::string>& args)
            -> commands::CommandResult;
     
        auto fan(cbdc::client& client, const std::vector<std::string>& args)
            -> commands::CommandResult;
     
        auto newaddress(cbdc::client& client,const std::vector<std::string>& args) 
            -> commands::CommandResult;
     
        auto importinput(cbdc::client& client,
                              const std::vector<std::string>& args) -> commands::CommandResult;
                              
        auto confirmtx(cbdc::client& client,
                                const std::vector<std::string>& args) -> commands::CommandResult;
                                
                                
        auto info(cbdc::client& client,const std::vector<std::string>& args) -> commands::CommandResult;
        
        auto sync(cbdc::client& client,const std::vector<std::string>& args) -> commands::CommandResult;
    }
    
    class CommandParser{
        public:
            CommandParser();
        
            auto execute(cbdc::client &client,const std::vector<std::string>& cmd) -> commands::CommandResult;
            
        protected:
            void register_command(const std::string& name, 
                                  std::function<commands::Command> &&hwnd);
            
        private:
            std::unordered_map<std::string, std::function<commands::Command>> m_commands;
            std::function<commands::Command> m_help = [&](cbdc::client &client,const std::vector<std::string>& args) -> commands::CommandResult{
                UNUSED(args);
                std::ostringstream os;
                std::vector<std::string> hargs;
                hargs.emplace_back("");
                hargs.emplace_back("h");
                
                commands::CommandResult ret(commands::CommandResult::HELP,"Available commands:\n");
                
                for(const std::pair<const std::string,std::function<commands::Command>>& mn : m_commands) {
                    if(mn.first == "help"){
                        continue;
                    }
                    
                    ret.set(mn.first,static_cast<std::string>(mn.second(client,hargs)));
                }
                
                return ret;
            };
    };
}

#endif
