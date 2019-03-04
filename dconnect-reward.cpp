#include <eosiolib/eosio.hpp>

using namespace eosio;

CONTRACT dconnect_reward: public contract {
  public:
      using contract::contract;
      dconnect_reward( name receiver, name code, datastream<const char*> ds )
         : contract(receiver, code, ds)

      ACTION claim(name account) {
         require_auth(account);
    	   
         auto time = now();
         post_tables postusertab(_self, account.value);

         posttab.emplace( _self, [&]( auto& u ) {
             u.primary = pk;
             u.key = key;
             u.value = value;
             u.owner = account;
             u.app = app;
      	     u.created = now();
         });   
      }

      ACTION reward(name account, uint64_t primary, std::string value) {
         require_auth(account);
         auto itr = posttab.find(primary);
         eosio_assert(itr != posttab.end(), "not found");
         eosio_assert(itr->owner.value == account.value, "only the owner is allowed to modify this content");
         
         posttab.modify( itr, _self, [&]( auto& row ) {
           row.value = value;
           row.modified = now();
         });
         post_tables postusertab(_self, account.value);
         postusertab.modify( postusertab.find(primary), _self, [&]( auto& row ) {
           row.value = value;
           row.modified = now();
         });
         post_tables postapptab(_self, app.value);
         postapptab.modify( postapptab.find(primary), _self, [&]( auto& row ) {
           row.value = value;
           row.modified = now();
         });
      }    

      TABLE post_table {
         uint64_t primary;
         name key;
         name owner;
         std::string value;
         name app;
      	 uint32_t created;
      	 uint32_t modified;
         uint64_t primary_key() const { return primary; }
         uint64_t by_key() const { return key.value; }
         uint64_t by_owner() const { return owner.value; }
         uint64_t by_app() const { return app.value; }
      };

      typedef eosio::multi_index<"posts"_n, post_table,  eosio::indexed_by<"key"_n, eosio::const_mem_fun<post_table, uint64_t, &post_table::by_key>>,  eosio::indexed_by<"ownerkey"_n, eosio::const_mem_fun<post_table, uint64_t, &post_table::by_owner>>,  eosio::indexed_by<"appkey"_n, eosio::const_mem_fun<post_table, uint64_t, &post_table::by_app>>> post_tables;

      using claim_action     = action_wrapper<  "set"_n, &dconnect_reward::claim>;
      using reward_action    = action_wrapper< "edit"_n, &dconnect_reward::reward>;
      private:
        post_tables posttab;
};

EOSIO_DISPATCH( dconnect_reward, (claim)(reward) )
