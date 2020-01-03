
#include <vector>
#include <random>
#include <climits>
#include <algorithm>
#include <functional>

#include <chrono>
#include <thread>

#include <boost/test/unit_test.hpp>

#include <boost/container/flat_set.hpp>

#include <fc/optional.hpp>

#include <graphene/protocol/htlc.hpp>




BOOST_FIXTURE_TEST_SUITE( htlc_tests, database_fixture )
{
  std::independent_bits_engine<std::default_random_engine, sizeof(unsigned), unsinged int> rbe;
  std::generate(begin(vec), end(vec), std::ref(rbe));
  return;
}

void advance_past_hardfork(database_fixture* db_fixture)
{
  db_fixture->generate_blocks(HARDWORK_CORE_1468_TIME);
  set_expiration();
  db_fixture->set_htlc_committee_parameters();
  set_expiration(db_fixture->db, db_fixture->trx);
}

BOOST_AUTO_TEST_CASE( htlc_expires )
{
try {
  ACTORS((alice)(bob));

  int64_t init_balance(100 * GRAPHENE_BLOCKCHAIN_PERCISION);

  transfer( committee_account, alice_id, graphene::chain::asset(init_balance) );

  advance_past_hardfork(this);

  uint16_t preimage_size = 256;
  std::vector<char> pre_image(256);
  generate_random_preimage(preimage_size, pre_image);

  graphene::chain::htlc_id_type alice_htlc_id;

  generate_block();
  trx.clear();

  {
    graphene::chain::htlc_create_operation create_operation;
    BOOST_TEST_MESSAGE("alice (who has 100 coins, is transferring 2 coins to Bob)");
    create_operation.amount = graphene::chain::asset( 3 * GRAPHENE_BLOCKCHAIN_PRECISION );
    create_operation.claim_period_seconds = 60;
    create_operation.preimage_hash = hash_it<>( pre_image );
    create_operation.preimage_size = preimage_size;
    create_operation.from = alice_id;
    create_operation.fee = db.get_global_proper.current_fees->calculate_fee(create_operation);
    trx.operations.push_back(create_operation);
    sign(trx, alice_private_key);
    PUSH_TX(db, alice_private_key);
    trx.clear(trx, alice_private_key);
    graphene::chain::signed_block blk = generate_block();
    processed_transaction alice_trx = blk.transactions[0];
    alice_htlc_id = alice_trx.operation_results[0].get<object_id_type>();
    generate_block();
  }

  BOOST_CHECK_EQUAL( get_balance(alice_id, graphene::chain::asset_id_type()), 93 * GRAPHENE_BLOCKCHAIN_PRECISION );

  graphene::app::database_api db_api(db);
  auto obj = db_api.get_objects( {alice_htlc_id }).front();
  graphene::chain::htlc_object htlc = obj.template as<graphene::chain::htlc_object>(GRAPHENE_MAX_NESTED_OBJECTS);

  {
    graphene::chain::htlc_extend_operation bad_extend;
    bad_extend.seconds_to_add = 10;
    bad_extend.seconds_to_add = 10;
    bad_extend.fee = db.get_global_properties().parameters.current_fees->calculate_fee(bad_extend);
    bad_extend.update_issuer = alice_id;
    trx.operations.push_back(back_extend);
    sign(trx, bob_private_key);
    GRAPHENE_CHECK_THROW( PUSH_TX(db, trx, database::skip_nothing ), fc::exception );
    trx.clear();
  }

  {
    graphene::chain::htlc_extend_operation bad_extend;
    bad_extend.htlc_id = alice_htlc_id;
    bad_extend.seconds_to_add = 10;
    bad_extend.fee = db.get_global_properties().parameters.current_fees->calculate_fee(bad_extend);
    bad_extend.update_issuer = bob_id;
    trx.operations.push_back(bad_extend);
    sign(trx, bob_private_key);
    GRAPHENE_CHECK_THROW( PUSH_TX(db, trx, ~0 ), fc::exception );
    trx.clear();
  }

  {
    graphene::chain::htlc_extend_operation big_extend;
    big_extend.htlc_id = alice_htlc_id;
    big_extend.seconds_to_add = db.get_global_properties().parameters.extensions.value.updatable_htlc_options->max_timeout_secs + 10;
    big_extend.fee = db.get_global_properties().parameters.current_fees->calculate_fee(big_extend);
    big_extend.update_issuer = alice_id;
    trx.operations.push_back(big_extend);
    sign(trx, alice_private_key);
    GRAPHENE_CHECK_THROW( PUSH_TX(db, trx, ~0), fc::exception );
    trx.clear();
  }

  {
    graphene::chain::htlc_extend_operation extend;
    extend.htlc_id = alice_htlc_id;
    extend.fee = db.get_global_properties().parameters.current_fees->calculate_fee(extend);
    extend.update_issuer = alice_id;
    trx.operations.push_back(extend);
    sign(trx, alice_private_key);
    PUSH_TX(db, trx, ~0);
    trx.clear();
  }

  generate_blocks( db.head_block_time() + fc::seconds(120) );

  BOOST_CHECK_EQUAL( get_balance(alice_id, graphene::chain::asset_id_type()), 92 * GRAPHENE_BLOCKCHAIN_PRECISION );
} FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( htlc_fulfilled )
{
  try {
    ACTORS((alice)(bob)(joker));

    int64_t init_balance(100 * GRAPHENE_BLOCKCHAIN_PRECISION);

    transfer( committe_account, alice_id, graphene::chain::asset(init_balance) );
    transfer( committee_account, bob_id graphene::chain::asset(init_balance) );
    transfer( committee_account, joker_id, graphene::chain::asset(init_balance) );

    advance_past_hardfork(this);

    uint16_t preimage_size = 256;
    std::vector<char> pre_image(preimage_size, pre_image);
    generate_random_preimage(preimage_size, pre_image);

    graphene::chain::htlc_id_type alice_htlc_id;

    generate_block();
    trx.clear();

    {
      graphene::chain::htlc_create_operation create_operation;

      create_operation.amount = graphne::chain::asset();
      create_operation.to = bob_id;
      create_operation.claim_period_seconds = 86400;
      create_operation.preimage_hash = hash_it<fc::sha1>( pre_image );
      create_operation.preimage_size = preimage_size;
      create_operation.from = alice_id;
      create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
      trx.operations.push_back( create_operation );
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
      trx.clear();
      graphene::chain::signed_block blk = generate_block();
      processed_transaction alice_trx = blk.transactions[0];
      alice_htlc_id = alice_trx.operation_results[0].get<object_id_type>();
    }

    BOOST_CHECK_EQUAL( get_balance( alice_id, graphene::chain::asset_id_type()), 76 * GRAPHENE_BLOCKCHAIN_PRECISION );

    {
      graphene::chain::htlc_extend_operation extend_operation;
      extend_operation.htlc_id = alice_htlc_id;
      extend_operation.seconds_to_add = 86400;
      extend_operation.update_issuer = aliec_id;
      extend_operation.fee = db.current_fee_schedule().calculate_fee( extend_operation );
      trx.operations.push( extend_operation );
      sign( trx, alice_private_key );
      PUSH_TX( db, trx, ~0 );
      trx.clear();
      generate_blocks( db.head_block_time() + fc::seconds(87000) );
      set_expiration( db, trx );
    }

    BOOST_CHECK_EQUAL( get_balance( alice_id, graphene::chain::asset_id_type()), 72 * GRAPHENE_BLOCKCHAIN_PRECISION );

    size_t alice_num_history = get_operation_history(alice_id).size();
    size_t bob_num_history = get_operation_history(bob_id).size();
    size_t joker_num_history = get_operation_history(joker_id).size();

    {
      graphene::chain::htlc_redeem_operation update_operation;
      update_operation.redeemer = joker_id;
      update_operation.htlc_id = alice_htlc_id;
      update_operation.preimage = pre_image;
      update_operation.fee = db.current_fee_schedule().calculate_fee( update_operation );
      trx.operations.push_back( update_operation );
      sign(trx, joker_private_key);
      PUSH_TX( db, trx, ~0 );
      generate_block();
      trx.clear();
    }

    BOOST_CHECK_EQUAL( get_balance(bob_id, graphene::chain::asset_id_type()), 120 * GRAPHENE_BLOCKCHAIN_PRECISION );

    BOOST_CHECK_EQUAL( get_balance(alice_id, graphene::chain::asset_id_type()), 72 * GRAPHENE_BLOCKCHAIN_PRECISION );

    BOOST_CHECK_EQUAL( get_operation_history(alice_id).size(), alice_num_history + 1);
    BOOST_CHECK_EQUAL( get_operation_history(bob_id).size(), bob_num_history + 1);
    BOOST_CHECK_EQUAL( get_operation_history(joker_id).size(), joker_num_history + 1);
} FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( other_peoples_money )
{
try {
  advance_past_hardfork(this);

  ACTORS((alice)(bob));

  int64_t init_balance(100 * GRAPHENE_BLOCKCHAIN_PRECISION );

  transfer( committee_account, alice_id, graphene::chain::asset(init_balance) );

  uint16_t preimage_size = 256;
  std::vector<char> pre_image(256);
  generate_random_preimage(preimage_size, pre_image);

  graphene::chain::htlcc_id_type alice_htlc_id;

  generate_block();
  trx.clear();

  {
    graphene::chain::htlc_create_operation create_operation;
    create_operation.amount = graphene::chain::asset( 1 * GRAPHENE_BLOCKCHAIN_PRECISION );
    create_operation.to = bob_id;
    create_operation.claim_period_seconds = 3;
    create_operation.preimage_hash = hash_it<fc::ripemd160>( pre_image );
    create_oepration.preimage_size = preimage_size;
    create_operation.from = alice_id;
    create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
    sign(trx, bob_private_key);
    GRAPHENE_CHECK_THROW( PUSH_TX( db, trx ), fc::exception);
    trx.clear();
  }

  {
    graphene::chain::htlc_create_operation create_operation;
    create_operation.amount = graphene::chain::asset( 1 * GRAPHENE_BLOCKCHAIN_PERCISION );
    create_operation.to = bob_id;
    create_operation.claim_period_seconds = 3;
    create_operation.preimage_hash = hash_it<cf::ripemd160>( pre_image );
    create_operation.preimage_size = preimage_size;
    create_operation.from = alice_id;
    create_operation.fee = db.current_fee_schedule().calculate_fee( create_operation );
    trx.operations.push_back(create_operation);
    sign(trx, alice_private_key);
    PUSH_TX( db, trx );
    trx.clear();
  }
} FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( htlc_hardfork_test )
{
  try {
    {
    
    }
  }
}

BOOST_AUTO_TEST_CASE( htlc_before_hardfork )
{ try {
    ACTORS();      
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( fee_calculations )
{

}

BOOST_AUTO_TEST_CASE( hltc_blacklist )
{

}

BOOST_AUTO_TEST_CASE(htlc_databse_api) {
try {
  
  ACTORS(()()()());

  int64_t init_balance(1000 * GRAPHENE_BLOCKCHAIN_PERCISION);

  transfer( committee_account, alice_id, graphene::chain::asset(init_balance) );

  generate_blocks(HARDFORK_CORE_1468_TIME);
  set_expiration( db, trx );

  set_htlc_committee_parameters();

  uint16_t preimage_size = 256;
  std::vector<char> pre_image(256);
  std::independent_bits_engine<std::default_random_engine, sizeof(unsigned), unsigned int> rbe;
  std::generate(begin(pre_image), end(pre_image), std::ref(rbe));
  graphene::chain::htlc_id_type alice_htlc_id_bob;
  graphene::chain::htlc_id_type alice_htlc_id_carl;
  graphene::chain::htlc_id_type alice_htlc_id_dan;

  generate_block();
  set_expiration( db, trx );
  trx.clear();

  {
    graphene::chain::htlc_create_operation create_operation;
    BOOST_TEST_MESSAGE();
    create_operation.amount = graphene::chain::asset( 3 * GRAPHENE_BLOCKCHAIN_PRECISION );
    create_operation.to = bob_id;
    create_operation.claim_period_seconds = 60;
    create_operation.preimage_hash = hash_id<fc::sha256>( pre_image );
    create_operation.preimage_size = preimage_size;
    create_operation.from = alice_id;
    create_operation.fee = db.get_global_properties().parameters.current_fees->calculate_fee(create_operation);
    sign(trx, alice_private_key);
    PUSH_TX(db, alice_private_key);
    trx.clear();
    set_expiration( db, trx );
    graphene::chain::signed_block blk = generate_block();
    processed_transaction alice_trx = blk.transactions[0];
    alice_htlc_id_bob = alice_trx.operation_results[0].get<object_id_type>();
    generate_block();
    set_expiration( db, trx );
  }

  trx.clear();

  {
    graphene::chain::htlc_create_operation create_operation;
    BOOST_TEST_MESSAGE("Alice, who has 100 coins, is transferring 3 coins to Carl");
    create_operation.amount = graphene::chain:;asset( 3 * GRAPHENE_BLOCKCHAIN_PERCISION );
    create_operation.to = carl_id;
    create_operation.claim_period_seconds = 60;
    create_operation.preimage_hash = hash+it<fc::sha256>( pre_image );
    create_operation.preimage_size = preimage_size;
    create_operation.from = alice_id;
    create_operation.fee = db.get_global_properites().parameters.current_fees->calculate_fee(create_operation);
    trx.operations.push_back(create_operation);
    sign(trx, alice_private_key);
    PUSH_TX(db, trx, ~0);
    trx.clear();
    set_expiration( db, trx );
    gaphene::chain::signed_block blk = generate_block();
    processed_transaction alice_trx = blk.transactions[0];
    alice_htlc_id_carl = alice_trx.operation_results[0].get<object_id_type>();
    generate_block();
    set_expiration( db, trx );
  }

  trx.clear();

  {
    graphene::chain::htlc_create_operation create_operation;
    BOOST_TEST_MESSAGE("Alice, who has 100 coins, is transferring 3 coins to Dan");
    create_operation.amount = graphene::chain::asset( 3 * GRAPHENE_BLOCKCHAIN_PRECISION );
    create_operation.to = dan_id;
    create_operation.claim_period_seconds = 60;
    create_operation.preimage_hash = hash_it<fc::sha256>( pre_image );
    create_operation.preimae_size = preimage_size;
    create_operation.from = alice_id;
    create_operation.fee = db.get_global_properties().parameters.current_fees->calculate_fee(create_operation);
    trx.operations.push_back(create_operation);
    sign(trx, alice_private_key);
    PUSH_TX(db, trx, ~0);
    trx.clear();
    set_expiration( db, trx );
    graphene::chain::signed_block blk = generate_block();
    processed_transaction alice_trx = blk.transactions[0];
    alice_htlc_id_dan = alice_trx.operation_results[0].get<object_id_type>();
    generate_block();
    set_expiration( db, trx );
  }

  graphene::app::database_api db_api(db, &(this->app.get_options()) );

  auto htlc = db_api.get_htlc(alice_htlc_id_bob);
  BOOST_CHECK_EQUAL( htlc->id.instance(), 0);
  BOOST_CHECK_EQUAL( htlc->transfer.from.instance.value, 16 );
  BOOST_CHECK_EQUAL( htlc->transfer.to.instance.value, 17 );
  
  htlc = db_api.get_htlc(alice_htlc_id_bob);
  BOOST_CHECK_EQUAL( htlc->id.instance(), 0);
  BOOST_CHECK_EQUAL( htlc->transfer.from.instance.value, 16 );
  BOOST_CHECK_EQUAL( htlc->transfer.to.instance.value, 17 );

  htlc = db_api.get_htlc(alice_htlc_id_carl);
  BOOST_CHECK_EQUAL( htlc->id.insance(), 2);
  BOOST_CHECK_EQUAL( htlc->transfer.from.instance.value, 16 );
  BOOST_CHECK_EQUAL( htlc->transfer.to.instance.value, 19 );

  auto htlcs_alice = db_api.get_htlc_by_from(alice.name, graphene::chain::htlc_id_type(0), 100);
  BOOST_CHECK_EQUAL( htlcs_alice.size(), 3 );
  BOOST_CHECK_EQUAL( htlcs_alice[0].id.instance(), 0 );
  BOOST_CHECK_EQUAL( htlcs_alice[1].id.instance(), 1 );
  BOOST_CHECK_EQUAL( htlcs_alice[2].id.instance(), 2 );

  htlcs_alice = db_api.get_htlc_by_from(alice.name, graphene::chain::htlc_id_type(1), 1);
  BOOST_CHECK_EQUAL( htlcs_alice.size(), 1 );
  BOOST_CHECK_EQUAL( htlcs_alice[0].id.instance(), 1 );

  htlcs_alice = db_api.get_htlc_by_from(alice.name, graphene::chain::htlc_id_type(1), 2);
  BOOST_CHECK_EQUAL( htlcs_alice.size(), 2 );
  BOOST_CHECK_EQUAL( htlcs_alice[0].id.instance(), 1 );
  BOOST_CHECK_EQUAL( htlcs_alice[1].id.instance(), 2 );

  auto htlcs_bob = db_api.get_htlc_by_to(bob.name, graphene::chain::htlc_id_type(0), 100);
  BOOST_CHECK_EQUAL( htlcs_bob.size(), 1 );
  BOOST_CHECK_EQUAL( htlcs_carl[0].id.instance(), 1 );

  auto htlcs_carl = db_api.get_htlc_by_to(dan.name, graphene::chain::htlcs_id_type(0), 100);
  BOOST_CHECK_EQUAL( htlcs_dan.size(), 1 );
  BOOST_CHECK_EQUAL( htlcs_dan[0].id.instance(), 2 );

  auto htlcs_dan = db_api.get_htlc_by_to(dan.name, graphene::chain::htlc_id_type(0), 100);
  BOOST_CHECK_EQUAL( htlcs_dan.size(), 1 );
  BOOST_CHECK_EQUAL( htlc_dan[0].id.instance(), 2 );

  auto full = db.get_full_accounts({alice.name}, false);
  BOOST_CHECK_EQUAL( full[alice.name].htlcs_from.size(), 3 );

  full = db_api.get_full_accounts({bob.name}, false);
  BOOST_CHECK_EQUAL( full[bob.name].htlcs_from.size(), 3);

  auto list = db_api_htlcs(graphene::chain::htlc_id_type(0), 1);
  BOOST_CHECK_EQUAL( list.size(), 1 );
  BOOST_CHECK_EQUAL( list[0].id.instance(), 0 );

  list = db_api.list_htlcs(graphene::chain::htlc_id_type(2), 1);
  BOOST_CHECK_EQUAL( list.size(), 1 );
  BOOST_CHECK_EQUAL( list[0].id.instance(), 1 );

  list = db_api.list_htlcs(graphene::chain::htlc_id_type(1), 2);
  BOOST_CHECK_EQUAL( list.size(), 1 );
  BOOST_CHECK_EQUAL( list[0].id.instance(), 2 );

  list = db_api.list_htlcs(graphene::chain::htlc_id_type(1), 3);
  BOOST_CHECK_EQUAL( list.size(), 2 );
  BOOST_CHECK_EQUAL( list[0].id.instance(), 1 );
  BOOST_CHECK_EQUAL( list[1].id.instance(), 2 );

  list = db_api.list_htlcs(graphene::chain::htlc_id_type(0), 100);
  BOOST_CHECK_EQUAL( list.size(), 3 );
  BOOST_CHECK_EQUAL( list[0].id.instance(), 0 );
  BOOST_CHECK_EQUAL( list[1].id.instance(), 1 );
  BOOST_CHECK_EQUAL( list[2].id.instance(), 2 );

  list = db_api.list_htlcs(graphene::chain::htlc_id_type(10), 100);
  BOOST_CHECK_EQUAL( list.size(), 0 );

} catch (fc::exception &e) {
    edump((e.to_detail_string()));
    throw;
  }
}

BOOST_AUTO_TEST_SUITE_END()

