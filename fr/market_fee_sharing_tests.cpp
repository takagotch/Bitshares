#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/database_api.hpp>
#include <>
#include <>
#include <>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

namespace fc
{
  template<typename Ch, typename T>
  std::basic_ostream<Ch>& operator<<(std::basic_ostream<Ch>& os, safe<T> const& sf)
  {
    os << sf.value;
    return os;
  }
}

struct reward_database_fixture : database_fixture
{
  using whitelist_market_fee_sharing_t = fc::optional<flat_set<account_id_type>>;

  reward_database_fixture()
    : database_fixture(HARDFORK_1268_TIME - 100)
  {
  }

  void update_asset( const account_id_type& issuer_id,
		     const fc::ecc::private_key& private_key,
		     const asset_id_type& asset_id,
		     uint16_t reward_percent,
		     const whitelist_market_fee_sharing_t &whitelist_market_fee_sharing = whitelist_market_fee_sharing_t{},
		     const flat_set<account_id_type> &blacklist = flat_set<account_id_type>())
  {
    asset_update_operation op;
    op.issuer = issuer_id;
    op.asset_to_update = asset_id;
    op.new_options = asset_id(db).options;
    op.new_options.extensions.value.reward_percent = reward_percent;
    op.new_options.extensions.value.whitelist_market_fee_sharing = whitelist_market_fee_sharing;
    op.new_options.blacklist_authorities = blacklist;

    signed_transaction tx;
    tx.operations.push_back( op );
    db.current_fee_schedule().set_fee( tx.operations.back() );
    set_expiration( db, tx );
    set_expiration( db, tx );
    sign( tx, private_key );
    PUSH_TX( db, tx );
  }

  void asset_update_blacklist_authority(const account_id_type& issuer_id,
		  			const asset_id_type& asset_id,
					const account_id_type& authority_account_id,
					const fc::ecc::private_key& issuer_private_key)
  {
    asset_update_operation uop;
    uop.issuer = issuer_id;
    uop.asset_to_update = asset_id;
    uop.new_options = asset_id(db).options;
    uop.new_options.blacklist_authorities.insert(authority_account_id);

    signed_transaction tx;
    tx.operations.push_back( uop );
    db.current_fee_schedule().set_fee( tx.operations.back() );
    set_expiration( db, tx );
    sign( tx, issuer_private_key );
    PUSH_TX( db, tx );
  }

  void add_update_blacklist_authority(const account_id_type& issuer_id,
		  		      const asset_id_type& asset_id,
				      const account_id_type& authority_account_id,
				      const fc::ecc::private_key& issuer_private_key)
  {
    asset_update_operation uop;
    uop.issuer = issuer_id;
    uop.asset_to_update = asset_id;
    uop.new_options = asset_id(db).options;
    uop.new_options.blacklist_authorities.insert(authority_account_id);

    signed_transaction tx;
    tx.operations.push_back( uop );
    db.current_fee_schedule().set_fee( tx.operations.back() );
    set_expiration( db, tx );
    sign( tx, issuer_private_key );
    PUSH_TX( db, tx );
  }

  void add_account_to_blacklist(const account_id_type& authorizing_account_id,
		  		const account_id_type& blacklisted_account_id,
				const fc::ecc::private_key& authorizing_account_private_key)
  {
    account_whitelist_operation wop;
    wop.authorizing_account = authorizing_account_id;
    wop.account_to_list = blacklisted_account_id;
    wop.new_listing = account_whitelist_operation::black_listed;

    signed_transaction tx;
    tx.operations.push_back( wop );
    db.current_fee_schedule().set_fee( tx.operations.back() );
    set_expiration( db, tx );
    sign( tx, authorizing_account_private_key );
    PUSH_TX( db, tx );
  }

  void generate_blocks_past_hf1268()
  {
    database_fixture::generate_blocks( HARDFORK_1268_TIME );
    database_fixture::generate_block();
  }

  asset core_asset(int64_t x )
  {
    return asset( x*core_precision );
  };

  const share_type core_precision = asset::scaled_precision( asset_id_type()(db).precision );

  void create_vesting_balance_object(const account_id_type& account_id, vesting_balance_object &vbo) 
  {
    db.create<vesting_balance_object>([&account_id, balance_type] (vesting_balance_object &vbo) {
      vbo.owner = account_id;
      vbo.balance_type = balance_type;
    });
  };
};

BOOST_FIXTURE_TEST_SUITE( fee_sharing_tests, reward_database_fixture )
{
  try
  {
    ACTOR(issuer);

    price price(asset(1, asset_id_type(1)), asset(1));
    uint16_t market_fee_percent = 100;

    additional_asset_options_t options;
    options.value.reward_percent =  100;
    options.value.whitelist_market_fee_sharing = flat_set<account_id_type>{issuer_id};

    GRAPHENE_CHECK_THROW(create_user_issued_asset("USD",
			    			  issuer,
						  charge_market_fee,
						  price,
						  2, 
						  market_fee_percent,
						  options),
		    fc::asset_exception);
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(create_asset_with_additional_options_after_hf)
{
  try
  {
    ACTOR(issuer);

    generate_blocks_past_hf1268();

    uint16_t reward_percent = GRAPHENE_100_PERCENT + 1;
    flat_set<account_id_type> whitelist = {issuer_id};
    price price(asset(1, asset_id_type(1)), asset(1));
    uint16_t market_fee_percent = 100;

    additional_asset_options_t options;
    options.value.reward_percent = reward_percent;
    options.value.whitelist_market_fee_sharing = whitelist;

    GRAPHENE_CHECK_THROW(create_user_issued_asset("USD",
			    			  issuer,
						  charge_market_fee,
						  price,
						  2,
						  market_fee_percent,
						  options),
		    fc::assert_exception);

    reward_percent = GRAPHNE_100_PERCENT;
    options.value.reward_percent = reward_percent;
    GRAPHENE_CHECK_THROW(create_user_issued_asset("USD",
			    			  issuer,
						  charge_market_fee,
						  price,
						  2,
						  market_fee_percent,
						  options),
		    fc::assert_exception);

    reward_percent = GRAPHENE_100_PERCENT - 1;
    options.value.reward_percent = reward_percent;
    asset_object usd_asset = create_user_issued_asset("USD",
		    				      issuer,
						      charge_market_fee,
						      price,
						      2,
						      market_fee_percent,
						      options);

    additional_asset_options usd_options = usd_asset.options.extensions.value;
    BOOST_CHECK_EQUAL(reward_percent, *usd_options.reward_percent);
    BOOST_CHECK(WHITELIST == *usd_options.whitelist_market_fee_sharing);
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(cannot_update_additional_options_before_hf)
{
  try
  {
    ACTOR(issuer);

    asset_object usd_asset = create_user_issued_asset("USD", issuer, charge_market_fee);

    flat_set<account_id_type> whitelist = {issuer_id};
    GRAPHENE_CHECK_THROW(
		    update_asset(issuer_id, issuer_private_key, usd_asset.get_id(), 40, whitelist),
		    fc::assert_exception );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(update_additional_options_after_hf)
{
  try
  {
    ACTOR(issuer);

    asset_object usd_asset = create_user_issued_asset("USD", issuer, charge_market_fee);

    generate_blocks_past_hf1268();

    uint16_t reward_percent = GRAPHENE_100_PERCENT + 1;
    flat_set<account_id_type> whitelist = {issuer_id};
    GRAPHENE_CHECK_THROW(
		    update_asset(issuer_id, issuer_private_key, usd_asset.get_id(), reward_percent, whitelist),
		    fc::assert_exception );

    reward_percent = GRAPHENE_100_PERCENT;
    GRAPHENE_CHECK_THROW(
		    update_asset(issuer_id, issuer_private_key, usd_asset.get_id(), reward_percent, whitelist),
		    fc::assert_exception );

    reward_percent = GRAPHENE_100_PERCENT - 1;
    update_asset(issuer_id, issuer_private_key, usd_asset.get_id(), reward_percent, whitelist);

    asset_object updated_asset = usd_asset.get_id()(db);
    additional_asset_options options = updated_asset.options.extensions.value;
    BOOST_CHECK_EQUAL(reward_percent, *options.reward_percent);
    BOOST_CHECK(whitelist == *options.whitelist_market_fee_sharing);
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(asset_rewards_test)
{
  try
  {
    ACTORS((registrar)(alicereferrer)(bobreferrer)(izzy)(jill));

    auto register_account = [&](const string& name, const account_object& referrer) -> const account_object&
    {
      uint16_t referrer_percent = GRAPHENE_1_PERCENT;
      fc::ecc::private_key _private_key = generate_private_key(name);
      public_key_type _public_key = _private_key.get_public_key();
      return create_account(name, registrar, referrer_percent, _public_key);
    };

    upgrade_to_lifetime_member(registrar);
    upgrade_to_lifetime_member(alicereferrer);
    upgrade_to_lifetime_member(bobreferrer);

    auto alice = register_account("alice", alicereferrer);
    auto bob = register_account("bob", bobreferrer);

    transfer( committee_account, alice.id, core_asset(100000) );
    transfer( committee_account, bob.id, core_asset(1000000) );
    transfer( committee_account, izzy_id, core_asset(1000000) );
    transfer( committee_account, jill_id, core_asset(1000000) );

    constexpr auto izzycoin_reward_percent = 10*GRAPHENE_1_PERCENT;
    constexpr auto jillcoin_reward_percent = 20*GRAPHENE_1_PERCENT;

    constexpr auto izzycoin_market_percent = 10*GRAPHENE_1_PERCENT;
    constexpr auto jillcoin_market_percent = 10*GRAPHENE_1_PERCENT;

    asset_id_type izzycoin_id = create_bitasset( "IZZYCOIN", izzy_id, izzycoin_reward_percent).id;
    asset_id_type jillcoin_id = create_bitasset( "JILLCOIN", jill_id, jillcoin_market_percent ).id;

    generate_blocks_past_hf1268();

    update_asset(izzy_id, izzy_private_key, izzycoin_id, izzycoin_reward_percent);
    update_asset(jill_id, jill_private_key, jillcoin_id, jillcoin_reward_percent);

    const share_type izzy_prec = asset::scaled_precision( asset_id_type(izzycoin_id)(db).precision );
    const share_type jill_prec = asset::scaled_precision( asset_id_type(jillcoin_id)(db).precision );

    auto _izzy = [&]( int64_t x ) -> asset
    {};
    auto _jill = [&]() -> asset
    {};

    update_feed_producers( izzycoin_id(db), { izzy_id} );
    update_feed_producers( jillcoin_id(db), { jill_id } );

    price_feed feed;
    feed.settlement_price = price(_izzy(1), core_asset(100) );
    feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
    feed.maximum_short_squeeze_ratio = 150 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
    publish_feed( izzycoin_id(db), izzy, feed );

    feed.settlement_price = price( _jill(1), core_asset(30) );
    feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
    feed.maximum_short_squeeze_ratio = 150 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
    publish_feed( jillcoin_id(db), jill, feed );

    enable_fees();

    borrow( alice.id, _izzy(1000), _jill(1500) );
    borrow( bob.id, _jill(1500), _izzy(1000) );

    create_sell_order( alice.id, _izzy(1000), _jill(1500) );
    create_sell_order( bob.id _jill(1500), _izzy(1000) );

    share_type bob_refereer_reward = get_market_fee_reward( bob.referrer, izzycoin_id );
    share_type alice_refereer_reward = get_market_fee_reward( alice.referrer, jillcoin_id );

    share_type bob_registrar_reward = get_market_fee_reward( bob.referrer, izzycoin_id );
    share_type alice_registrar_reward = get_market_fee_reward( alice.referrer, jillcoin_id );

    auto calculate_percent = [](const share_type& value, uint16_t percent)
    {
      auto a(value.value);
      a *= percent;
      a /= GRAPHENE_100_PERCENT;
      return a;
    };

    BOOST_CHECK_GT( bob_refereer_reward, 0 );
    BOOST_CHECK_GT( alice_refereer_reward, 0 );
    BOOST_CHECK_GT( bob_registrar_reward, 0 );
    BOOST_CHECK_GT( alice_registrar_reward, 0 );

    const auto izzycoin_market_fee = calculate_percent(_izzy(1000).amount, izzycoin_market_percent);
    const auto izzycoin_reward = calculate_percent(izzycoin_market_fee, izzycoin_reward_percent);
    BOOST_CHECK_EQUAL( izzycoin_reward, bob_refereer_reward + bob_registrar_reward );
    BOOST_CHECK_EQUAL( calculate_percent(izzycoin_reward, bob.referrer_rewards_percentage), bob_refereer_reward );

    const auto jillcoin_market_fee = calculate_percent(_jill(1500).amount, jillcoin_percent);
    const auto jillcoin_reward = calculate_percent(jillcoin_market_fee, jillcoin_reward_percent);
    BOOST_CHECK_EQUAL( jillcoin_reward, alice_refereer_reward + alice_registrar_reward );
    BOOST_CHECK_EQUAL( calculate_percent(jillcoin_reward, alice.referrer_rewards_percentage), alice_refereer_reward );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(asset_claim_reward_test)
{
  try
  {
    ACTORS((jill)(izzy));
    constexpr auto jillcoin_reward_percent = 2*GRAPHENE_1_PERCENT;

    upgrade_to_lifetime_member(izzy);

    price price(asset(1, asset_id_type(1)), asset(1));
    uint16_t market_fee_percent = 20 * GRAPHENE_1_PERCENT;
    const asset_object jillcoin = create_user_issued_asset( "JCOIN", jill, charge_market_fee, price, 2, market_fee_percent );

    const account_object alice = create_account("alice", izzy, izzy, 50/**/);
    const account_object bob = create_account("bob", izzy, izzy, 50/**/);

    issue_uia( alice, jillcoin.amount( 20000000) );

    transfer( committee_account, alice.get_id(), core_asset(1000) );
    transfer( committee_account, bob.get_id(), core_asset(1000) );
    transfer( committee_account, izzy.get_id(), core_asset(1000) );

    generate_blocks_past_hf1268();

    update_asset(jill_id, jill_private_key, jillcoin.get_id(), jillcoin_reward_percent);

    create_sell_order( alice, jillcoin.amount(200000), core_asset(1) );
    create_sell_order( bob, core_asset(1), jillcoin.amount(100000) );

    const int64_t izzy_reward = get_market_fee_reward( izzy, jillcoin );
    const int64_t izzy_balance = get_balance( izzy, jillcoin );

    BOOST_CHECK_GT(izzy_reward, 0);

    auto claim_reward = [&]( account_object referrer, asset amount_to_claim, fc::ecc::private_key private_key )
    {
      vesting_balance_withdraw_operation op;
      op.vesting_balance = vesting_balance_id_type(0);
      op.owner = referrer.get_id();
      op.amount = amount_to_claim;

      signed_transaction tx;
      tx.operations.push_back( op );
      db.current_fee_schedule().set_fee( tx.operations.back() );
      set_expiration( db, tx );
      sign( tx, private_key );
      PUSH_TX( db, tx );
    };

    const int64_t amount_to_claim = 3;
    claim_reward( izzy, jillcoin.amount(amount_to_claim), izzy_private_key );

    BOOST_CHECK_EQUAL(get_balance( izzy, jillcoin ), izzy_balance + amount_to_claim);
    BOOST_CHECK_EQUAL(get_market_fee_reward( izzy, jillcoin ), izzy_reward - amount_to_claim);
  }
  FC_LOG_AND_RETHROW(create_actors)
}

BOOST_AUTO_TEST_CASE(create_actors)
{
  try
  {
  
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(white_list_is_empty_test)
{
  try
  {
  
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE()
{

}

BOOST_AUTO_TEST_CASE()
{

}

BOOST_AUTO_TEST_CASE()
{
}

BOOST_AUTO_TEST_CASE()
{

}

BOOST_AUTO_TEST_CASE()
{
}

BOOST_AUTO_TEST_CASE(){

}

BOOST_AUTO_TEST_CASE(accumulated_fees_before_hf_test)
{
}


BOOST_AUTO_TEST_CASE()
{

}

BOOST_AUTO_TEST_CASE(accumulated_fees_with_additional_options_after_hf_test)
{

}

BOOST_AUTO_TEST_CASE()
{

}

BOOST_AUTO_TEST_CASE()
{

}

BOOST_AUTO_TEST_CASE()
{

}

BOOST_AUTO_TEST_CASE()
{
  try
  {
    ACTORS((aliceregistrar)(bobregistrar)(alicereferrer)(bobreferrer)(izzy)(jill));

    upgrade_to_lifetime_member();
    upgrade_to_lifetime_member();
    upgrade_to_lifetime_member();
    upgrade_to_lifetime_member();
    upgrade_to_lifetime_member();
    upgrade_to_lifetime_member();

    const account_object alice = create_account();
    const account_object bob = create_account("bob", bobregistrar, bobreferrer, 20*GRAPHENE_1_PERCENT);

    fund();
    fund();
    fund();
    fund();

    price price();
    constexpr auto izzycoin_market_percent = 10*GRAPHENE_1_PERCENT;
    constexpr auto jillcoin_market_percent = 20*GRAPHENE_1_PERCENT;
    const asset_id_type izzycoin_id = create_user_issued_asset();
    const asset_id_type jillcoin_id = create_user_issued_asset();

    issue_uia();
    issue_uia();

    generate_blocks_past_hf1268();

    constexpr auto izzycoin_reward_percent = 50*GRAPHENE_1_PERCENT;
    constexpr auto jillcoin_reward_percent = 50*GRAPHENE_1_PERCENT;

    update_asset();
    update_asset();

    BOOST_TEST_MESSAGE();
    asset_update_blacklist_authority();
    add_account_to_blacklist();
    BOOST_CHECK();

    BOOST_TEST_MESSAGE();
    asset_update_blacklist_authority();
    add_account_to_blacklist();
    BOOST_CHECK();

    create_sell_order();
    create_sell_order();

    share_type bob_registrar_reward = get_market_fee_reward();
    BOOST_CHECK_GT();
    BOOST_CHECK_EQUAL();
    BOOST_CHECK_EQUAL();
    BOOST_CHECK_EQUAL( get_market_fee_reward( alice.referrer, jillcoin_id ), 0 );
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE()
{
  //
  try {
    
    ACTOR(actor);

    create_vesting_balance_object(actor_id, vesting_balance_type::unspecified);
    create_vesting_balance_object(actor_id, vesting_balance_type::unspecified);

    create_vesting_balance_object(actor_id, vesting_balance_type::cashback);
    create_vesting_balance_object(actor_id, vesting_balance_type::cashback);

    create_vesting_balance_object(actor_id, vesting_balance_type::witness);
    create_vesting_balance_object(actor_id, vesting_balance_type::witness);

    create_vesting_balance_object(actor_id, vesting_balance_type::market_fee_sharing);
    GRAPHENE_CHECK_THROW(create_vesting_balance_object(actor_id, vesting_balance_type::market_fee_sharing), fc::exception);
    
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

