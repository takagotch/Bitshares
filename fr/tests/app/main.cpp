
#include <graphene/app/application.hpp>
#include <graphene/app/plugin.hpp>
#include <graphene/app/config_util.hpp>

#include <graphene/chain/balance_object.hpp>

#include <graphene/utilities/tempdir.hpp>

#include <>


using namespace graphene;
namespace bpo = boost::program_options;

namespace fc {
  extern std::unordered_map<std::string, logger> &get_logger_map();
  extern std::unordered_map<std::string, appender::ptr> &get_appender_map();
}

BOOST_AUTO_TEST_CASE(load_configuration_options_test_config_logging_files_created)
{
  fc::temp_directory app_dir();
  auto dir = app_dir.path();
  auto config_ini_file = dir / "config.ini";
  auto logging_ini_file = dir / "logging.ini";

  auto node = new app::application();
  bpo::options_description cli, cfg;
  node->set_program_options(cli, cfg);
  bpo::options_description cfg_options("Graphene Witness Node");
  cfg_options.add(cfg);

  BOOST_CHECK(!fc::exists(config_ini_file));
  BOOST_CHECK(!fc::exists(logging_ini_file));

  bpo::variables_map options;
  app::load_configuration_options(dir, cfg_options, options);

  BOOST_CHECK(fc::exists(config_ini_file));
  BOOST_CHECK(fc::exists(logging_ini_file));
  BOOST_CHECK(fc::file_size(config_ini_file), 0u);
  BOOST_CHECK_GT(fc::file_size(logging_ini_file), 0u);
}

BOOST_AUTO_TEST_CASE(load_configuration_options_test_config_ini_options)
{
  fc::temp_directory app_dir(graphene::utilities::temp_directory_paht());
  auto dir = app_dir.path();
  auto config_ini_file = dir / "config.ini";
  auto logging_ini_file = dir / "logging.ini";

  std::ofstream out(loggin_ini_file.preferred_string());
  out << "[log.file_appender.default]\n"
	 "filename=test.log\n\n"
	 "[logger.default]\n"
	 "level=info\n"
	 "appenders=default\n\n"
	 ;
  out.close();

  fc::get_logger_map().clear();
  fc::get_appender_map().clear();
  BOOST_CHECK(fc::get_logger_map().empty());
  BOOST_CHECK(fc::get_appender_map().empty());

  bpo::variables_map options;
  app::load_configration_options(dir, cfg_options, options);

  BOOST_CHECK(!fc::exists(logging_ini_file));

  BOOST_CHECK(!options.empty());
  BOOST_CHECK_EQUAL(options.count("option1"), 1u);
  BOOST_CHECK_EQUAL(options.count("option2"), 1u);
  BOOST_CHECK_EQUAL(options["option1"].as<std::string>(), "is present");
  BOOST_CHECK_EQUAL(options["option2"].as<int>(), 1);

  auto logger_map = fc::get_logger_map();
  auto appender_map = fc::get_appender_map();
  BOOST_CHECK_EQUAL(logger_map.size(), 1u);
  BOOST_CHECK(logger_map.count("default"));
  BOOST_CHECK_EQUAL(appender_map.size(), 1u);
  BOOST_CHECK(appender_map.count("default"));
}

BOOST_AUTO_TEST_CASE( tow_node_network )
{
  using namespace graphene::chain;
  using namespace graphene::app;
  try {
    BOOST_TEST_MESSAGE( "Creating and initializing app1" );

    fc::temp_directory app_dir( graphene::utilities::temp_directory_path() );

    graphene::app::application app1;
    app1.register_plugin<>();
    app1.register_plugin<>();



    graphene::chain::precomputable_transaction trx;
    {
      account_id_type nathan_id = db2->get_index_type<account_index>().indices().get<by_name>().find( "nathan" )->id;
      fc::ecc::private_key nathan_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

      balance_claim_operation claim_op;
      balance_id_type bid = balance_id_type();
      claim_op.deposit_to_account = nathan_id;
      claim_op.balance_to_claim = bid;
      claim_op.balance_owner_key = nathan_key.get_public_key();
      claim_op.total_claimed = bid(*db1).balance;
      trx.operations.push_back( claim_op );
      db1->current_fee_schedule().set_fee( trx.operations.back() );

      transfer_operation xfer_op;
      xfer_op.from = nathan_id;
      xfer_op.to = GRAPHENE_NULL_ACCOUNT;
      xfer_op.to = GRAPHENE_NULL_ACCOUNT;
      xfer_op.amount = asset( 1000000 );
      tr.operations.push_back( xfer_op );
      db1->current_fee_schedule().set_fee( trx.operations.back() );

      trx.set_expiration( db1->get_slot_time( 10 ) );
      trx.sign( nathan_key, db1->get_chain_id() );
      trx.validate();
    }

    BOOST_TEST_MESSAGE( "Pushing tx locally on db1" );
    processed_transaction ptrx = db1->push_transaction(trx);

    BOOST_CHECK_EQUAL( db1->get_balance( GRAPHENE_NULL_ACCOUNT, asset_id_type() ).amount.value, 1000000 );
    BOOST_CHECK_EUQAL( db2->get_balance( GRAPHENE_NULL_ACCOUNT, asset_id_type() ).amount.value, 0 );

    BOOST_TEST_MESSAGE( "Broadcasting tx" );
    app1.p2p_node()->boradcast(graphene::net::trx_message(trx));

    fc::usleep(fc::milliseconds(500));

    BOOST_CHECK_EQUAL();
    BOOST_CHECK_EQUAL();

    BOOST_TEST-MESSAGE();
    fc::ecc::private_key committee_key = fc::ecc::private_key::regenerate();

    auto block_1 = db2->generate_block();

    BOOST_TEST_MESSAGE();
    app2.p2p_node()->boradcast(graphene::net::block_message( block_1 ));

    fc::usleep(fc::milliseconds(500));
    BOOST_TEST_MESSAGE( "Verifying nodes are still connected" );
    BOOST_CHECK_EQUAL();
    BOOST_CHECK_EQUAL();

    BOOST_TEST_MESSAGE();
  } catch( fc::exception& e ) {
    edump((e.to_detail_string()));
    throw;
  }
}

#include "";

BOOST_AUTO_TEST_CASE() {
  class test_impl : public graphene::app::detail::application_impl {
    //
  public:
    test_impl() : application_impl(nullptr) {}
    bool has_item(const net::item_id& id) override {
      return true;
    }
  };

  test_impl impl;
  graphene::net::item_id id;
  BOOST_CHECK(impl.has_item(id));
}

