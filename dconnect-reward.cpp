/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "dconnect-reward/dconnect-reward.hpp"

namespace eosio {
//on top of eos properties we add some for bounty management
void token::create( name   issuer,
                    asset  maximum_supply,
                    name bounty_contract,
                    asset bounty,
                    uint64_t bounty_rate
                    )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.bounty_contract = bounty_contract;
       s.bounty   	 = bounty;
       s.lastpay         = now(); //we record the starting point
       s.bounty_rate     = bounty_rate;
       s.supply.symbol   = maximum_supply.symbol;
       s.max_supply      = maximum_supply;
       s.issuer          = issuer;
    });
}


void token::issue( name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
      SEND_INLINE_ACTION( *this, transfer, { {st.issuer, "active"_n} },
        { st.issuer, to, quantity, memo }
      );
    }
}


void token::reward(  name to, name vote,asset quantity, string memo, int64_t content )
{
    require_auth( to );
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must use positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    sub_balance( to, quantity );
    payouts rewardstable( _self, name("rewards").value );
    rewardstable.emplace( _self, [&]( auto& a ) {
      a.pk = rewardstable.available_primary_key();
      a.vote = vote;
      a.content = content;
      a.to = to;
      a.memo = memo;
      a.time = now();
      a.quantity = quantity;
    });
}

void token::retire( name to,  asset quantity, string memo )
{
    require_auth( to );
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist" );
    const auto& st = *existing;

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must retire positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

    float_t quantity_amount  = (float_t)(quantity.amount);
    float_t supply_amount = (float_t)(st.supply.amount);
    float_t share = (float_t)(quantity_amount / supply_amount);
    uint64_t amount = st.bounty.amount*share;
    asset payout_asset = asset((uint64_t)4, st.bounty.symbol);
    payout_asset.amount = amount;

    print(" quantity_amount: ");
    print(quantity.amount);
    print(" supply_amount: ");
    print(st.supply.amount);
    print(" share: ");
    print(share);
    print(" amount: ");
    print(payout_asset.amount);
    eosio_assert(payout_asset.amount>0, "Not enough to claim with.");
    sub_balance( to, quantity );
    statstable.modify( st, same_payer, [&]( auto& s ) {
     s.supply -= quantity;
    });
    payouts payoutstable( _self, name("payouts").value );
    payoutstable.emplace( _self, [&]( auto& a ){
      a.pk = payoutstable.available_primary_key();
      a.bounty = payout_asset;
      a.to = to;
      a.memo = memo;
      a.time = now();
      a.quantity = quantity;
    });
}

void token::pay() {
    require_auth( _self );
    print("running payments\n");
    payouts payoutstable( _self, name("payouts").value);
    print("payouts table");
    payouts rewardstable( _self, name("rewards").value);
    print("rewards table");
    int done = false;
    print(done);
    for(auto itr = payoutstable.begin(); itr != payoutstable.end() && !done;) {
      print(done);
      print(itr->to);
      action(permission_level{ _self, name("active") },
       name("eosio.token"), name("transfer"),
       std::make_tuple( _self, itr->to, itr->bounty, itr->memo)
      ).send();
      itr = payoutstable.erase(itr);
      done = true;
    }
    print(done);
    for(auto itr = rewardstable.begin(); itr != rewardstable.end() && !done;) {
      print("processing reward\n"); 
      print(itr->to); 
      stats statstable( _self, itr->quantity.symbol.code().raw() );
      auto existing = statstable.find(  itr->quantity.symbol.code().raw() );
      eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
      const auto& st = *existing;

      if(now() < itr->time + 86400) {
	itr++;
	continue;
      }

      asset payout_asset = asset((uint64_t)4, itr->quantity.symbol);
      payout_asset.amount = itr->quantity.amount*1009/1000;
      add_balance( itr->to, payout_asset, _self );

      asset vote_asset = asset((uint64_t)4, itr->quantity.symbol);
      vote_asset.amount = (itr->quantity.amount*1001/1000)-itr->quantity.amount;
      add_balance( itr->vote, vote_asset, _self );

      asset add_asset = asset((uint64_t)4, itr->quantity.symbol);
      add_asset.amount = payout_asset.amount + vote_asset.amount - itr->quantity.amount;
      statstable.modify( st, same_payer, [&]( auto& s ) {
         s.supply += add_asset;
      });

      itr = rewardstable.erase(itr);
      done = true;
    }
    print(done);
    transaction out{};
    out.actions.emplace_back(permission_level{_self, name("active")}, _self, name("pay"), std::make_tuple());
    out.delay_sec = 60;
    out.send(0, _self, true);
}

void token::transfer( name    from,
                      name    to,
                      asset   quantity,
                      string  memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

    sub_balance( from, quantity );
    add_balance( to, quantity, payer );
}

void token::sub_balance( name owner, asset value ) {
   accounts from_acnts( _self, owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
}

void token::add_balance( name owner, asset value, name ram_payer )
{
   accounts to_acnts( _self, owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::open( name owner, const symbol& symbol, name ram_payer )
{
   require_auth( ram_payer );

   auto sym_code_raw = symbol.code().raw();

   stats statstable( _self, sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "symbol does not exist" );
   eosio_assert( st.supply.symbol == symbol, "symbol precision mismatch" );

   accounts acnts( _self, owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}

void token::close( name owner, const symbol& symbol )
{
   require_auth( owner );
   accounts acnts( _self, owner.value );
   auto it = acnts.find( symbol.code().raw() );
   eosio_assert( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   eosio_assert( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}

} /// namespace eosio

EOSIO_DISPATCH( eosio::token, (create)(issue)(transfer)(open)(close)(reward)(retire)(pay) )
