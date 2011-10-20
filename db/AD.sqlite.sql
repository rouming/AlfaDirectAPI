/*
 * !!! AUTOMATICALLY GENERATED BY THE SCRIPT !!!
 * !!! ALL MODIFICATIONS WILL BE OVERRIDEN !!!
 */

CREATE TABLE IF NOT EXISTS AD_SUB_ACCOUNTS (
	"acc_code"	VARCHAR(64)	NOT NULL,
	"nick_name"	VARCHAR(256),
	"treaty"	INTEGER,
	"toy"	VARCHAR(256),
	"allow_debet"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"unsecured_limit"	FLOAT,
	"start_margin"	FLOAT,
	"msg_margin"	FLOAT,
	"min_margin"	FLOAT,

	PRIMARY KEY ("acc_code")
) ;

CREATE TABLE IF NOT EXISTS AD_ACCOUNTS (
	"treaty"	INTEGER	NOT NULL,
	"full_name"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"start_margin"	FLOAT,
	"msg_margin"	FLOAT,
	"min_margin"	FLOAT,

	PRIMARY KEY ("treaty")
) ;

CREATE TABLE IF NOT EXISTS AD_ACTIVES (
	"p_code"	VARCHAR(64)	NOT NULL,
	"p_name"	VARCHAR(256),
	"ANSI_name"	VARCHAR(256),
	"expired"	VARCHAR(256),
	"serial_group"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"em_code"	VARCHAR(256),
	"at_code"	VARCHAR(256),
	"emission_no"	INTEGER,
	"transh_no"	INTEGER,
	"at_name"	VARCHAR(256),
	"reg_code"	VARCHAR(256),
	"buyback_price"	FLOAT,
	"buyback_date"	TIMESTAMP,
	"nominal_curr"	VARCHAR(256),
	"mult"	INTEGER,
	"divid"	INTEGER,
	"strike"	FLOAT,
	"contr_descr"	VARCHAR(256),

	PRIMARY KEY ("p_code")
) ;

CREATE TABLE IF NOT EXISTS AD_MY_CURRENT_TRADES (
	"trd_no"	INTEGER	NOT NULL,
	"paper_no"	INTEGER,
	"qty"	INTEGER,
	"price"	FLOAT,
	"ts_time"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"change"	INTEGER,
	"type"	INTEGER,

	PRIMARY KEY ("trd_no")
) ;

CREATE TABLE IF NOT EXISTS AD_BALANCE (
	"acc_code"	VARCHAR(64)	NOT NULL,
	"p_code"	VARCHAR(64)	NOT NULL,
	"place_code"	VARCHAR(64)	NOT NULL,
	"income_rest"	FLOAT,
	"real_rest"	FLOAT,
	"plan_rest"	FLOAT,
	"brutto_rest"	FLOAT,
	"turnover"	FLOAT,
	"balance_price"	FLOAT,
	"paper_no"	INTEGER,
	"i_last_update"	INTEGER,
	"forword_rest"	FLOAT,
	"assessed_price"	FLOAT,
	"real_vol"	FLOAT,
	"forword_vol"	FLOAT,
	"bal_forword_vol"	FLOAT,
	"profit_vol"	FLOAT,
	"treaty"	INTEGER,
	"prev_close"	FLOAT,
	"in_bids"	FLOAT,
	"in_offers"	FLOAT,
	"netto_rest"	FLOAT,
	"nkd"	FLOAT,
	"bought"	FLOAT,
	"sold"	FLOAT,
	"daily_pl"	FLOAT,
	"income_pl"	FLOAT,
	"income_vol"	FLOAT,
	"price_koef"	FLOAT,
	"reserved_for_backward_cap"	FLOAT,
	"pl"	FLOAT,
	"nkd_inc"	FLOAT,
	"curr_rate"	FLOAT,
	"price_koef_pl"	FLOAT,
	"var_margin"	FLOAT,
	"var_margin_pc"	FLOAT,

	PRIMARY KEY ("acc_code", "p_code", "place_code")
) ;

CREATE TABLE IF NOT EXISTS AD_BOARDS (
	"board_code"	VARCHAR(64)	NOT NULL,
	"place_code"	VARCHAR(256),
	"board_name"	VARCHAR(256),
	"tax"	FLOAT,
	"price_type"	VARCHAR(256),
	"i_last_update"	INTEGER,

	PRIMARY KEY ("board_code")
) ;

CREATE TABLE IF NOT EXISTS AD_MY_CHAT (
	"mes_no"	INTEGER	NOT NULL,
	"to_sys_name"	VARCHAR(256),
	"from_sys_name"	VARCHAR(256),
	"subject"	VARCHAR(256),
	"body"	VARCHAR(256),
	"show_window"	VARCHAR(256),
	"color"	INTEGER,
	"type"	VARCHAR(256),
	"end_date"	TIMESTAMP,
	"db_date"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"to_user_id"	INTEGER,
	"from_user_id"	INTEGER,

	PRIMARY KEY ("mes_no")
) ;

CREATE TABLE IF NOT EXISTS AD_CHAT_THEMES (
	"theme_code"	VARCHAR(64)	NOT NULL,
	"theme_name"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"disable_selection"	VARCHAR(256),

	PRIMARY KEY ("theme_code")
) ;

CREATE TABLE IF NOT EXISTS AD_CURR (
	"curr_code"	VARCHAR(64)	NOT NULL,
	"curr_name"	VARCHAR(256),
	"i_last_update"	INTEGER,

	PRIMARY KEY ("curr_code")
) ;

CREATE TABLE IF NOT EXISTS AD_EXCHANGE_CONTRAGENTS (
	"ex_code"	VARCHAR(64)	NOT NULL,
	"firm_code"	VARCHAR(64)	NOT NULL,
	"firm_name"	VARCHAR(256),
	"status"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"place_code"	VARCHAR(256),

	PRIMARY KEY ("ex_code", "firm_code")
) ;

CREATE TABLE IF NOT EXISTS AD_EMITENTS (
	"em_code"	VARCHAR(64)	NOT NULL,
	"short_name"	VARCHAR(256),
	"full_name"	VARCHAR(256),
	"short_name_eng"	VARCHAR(256),
	"ind_code"	VARCHAR(256),
	"www"	VARCHAR(256),
	"country"	VARCHAR(256),
	"i_last_update"	INTEGER,

	PRIMARY KEY ("em_code")
) ;

CREATE TABLE IF NOT EXISTS AD_EXCHANGES (
	"ex_code"	VARCHAR(64)	NOT NULL,
	"ex_name"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"country_code"	VARCHAR(256),

	PRIMARY KEY ("ex_code")
) ;

CREATE TABLE IF NOT EXISTS AD_MY_QUOTES (
	"paper_no"	INTEGER	NOT NULL,
	"open_price"	FLOAT,
	"last_price"	FLOAT,
	"avg_price"	FLOAT,
	"close_price"	FLOAT,
	"change"	FLOAT,
	"max_deal"	FLOAT,
	"min_deal"	FLOAT,
	"sum_qty"	FLOAT,
	"sum_volume"	FLOAT,
	"last_qty"	INTEGER,
	"no_deal"	INTEGER,
	"sell_qty"	INTEGER,
	"buy_qty"	INTEGER,
	"sell"	FLOAT,
	"buy"	FLOAT,
	"reserved_backward_cap"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"high_bid"	FLOAT,
	"low_offer"	FLOAT,
	"prev_price"	FLOAT,
	"yield"	FLOAT,
	"avg_yield"	FLOAT,
	"close_yield"	FLOAT,
	"price2prev_avg"	FLOAT,
	"buy_sqty"	INTEGER,
	"buy_count"	INTEGER,
	"sell_sqty"	INTEGER,
	"sell_count"	INTEGER,
	"status"	INTEGER,
	"market_watch"	FLOAT,
	"euro_spread"	FLOAT,
	"open_pos_qty"	INTEGER,
	"volatility"	FLOAT,
	"theor_price"	FLOAT,
	"assessed_price"	FLOAT,

	PRIMARY KEY ("paper_no")
) ;

CREATE TABLE IF NOT EXISTS AD_PAPER_STATUSES (
	"status"	VARCHAR(64)	NOT NULL,
	"description"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"num_status"	INTEGER,

	PRIMARY KEY ("status")
) ;

CREATE TABLE IF NOT EXISTS AD_ABNORMAL_DATES (
	"ab_date"	TIMESTAMP	NOT NULL,
	"i_last_update"	INTEGER,

	PRIMARY KEY ("ab_date")
) ;

CREATE TABLE IF NOT EXISTS AD_NEWS (
	"new_no"	INTEGER	NOT NULL,
	"stripe_code"	VARCHAR(256),
	"provider"	VARCHAR(256),
	"db_data"	TIMESTAMP,
	"topic"	VARCHAR(256),
	"subject"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"important"	VARCHAR(256),

	PRIMARY KEY ("new_no")
) ;

CREATE TABLE IF NOT EXISTS AD_MY_REQUEST_TYPES (
	"ord_type"	VARCHAR(64)	NOT NULL,
	"description"	VARCHAR(256),
	"i_last_update"	INTEGER,

	PRIMARY KEY ("ord_type")
) ;

CREATE TABLE IF NOT EXISTS AD_ORDER_STATUSES (
	"ord_status"	VARCHAR(64)	NOT NULL,
	"description"	VARCHAR(256),
	"i_last_update"	INTEGER,

	PRIMARY KEY ("ord_status")
) ;

CREATE TABLE IF NOT EXISTS AD_PAPERS (
	"paper_no"	INTEGER	NOT NULL,
	"p_code"	VARCHAR(256),
	"place_code"	VARCHAR(256),
	"ts_p_code"	VARCHAR(256),
	"board_code"	VARCHAR(256),
	"lot_size"	INTEGER,
	"decimals"	INTEGER,
	"nkd"	FLOAT,
	"cup_size"	FLOAT,
	"cup_date"	TIMESTAMP,
	"cup_period"	INTEGER,
	"nominal"	FLOAT,
	"mat_date"	TIMESTAMP,
	"allow_pawn"	VARCHAR(256),
	"sum_limit"	FLOAT,
	"i_last_update"	INTEGER,
	"expired"	VARCHAR(256),
	"allow_short"	VARCHAR(256),
	"firm_code"	VARCHAR(256),
	"mk_short"	FLOAT,
	"mk_long"	FLOAT,
	"mk_short_min"	FLOAT,
	"mk_long_min"	FLOAT,
	"mk_short_repo"	FLOAT,
	"mk_long_repo"	FLOAT,
	"price_step"	FLOAT,
	"create_date"	INTEGER,
	"base_paper_no"	INTEGER,
	"just_inserted"	INTEGER,
	"go_buy"	FLOAT,
	"go_sell"	FLOAT,
	"go_covered"	FLOAT,
	"go_uncovered"	FLOAT,
	"price_step_cost"	FLOAT,

	PRIMARY KEY ("paper_no")
) ;

CREATE TABLE IF NOT EXISTS AD_POSITIONS (
	"acc_code"	VARCHAR(64)	NOT NULL,
	"place_code"	VARCHAR(64)	NOT NULL,
	"depo_acc"	VARCHAR(256),
	"i_last_update"	INTEGER,

	PRIMARY KEY ("acc_code", "place_code")
) ;

CREATE TABLE IF NOT EXISTS AD_MY_QUOTE_QUEUES (
	"paper_no"	INTEGER	NOT NULL,
	"price"	FLOAT,
	"buy_qty"	INTEGER,
	"sell_qty"	INTEGER,
	"i_last_update"	INTEGER,
	"yield"	FLOAT,

	PRIMARY KEY ("paper_no")
) ;

CREATE TABLE IF NOT EXISTS AD_ORDERS (
	"ord_no"	INTEGER	NOT NULL,
	"acc_code"	VARCHAR(256),
	"type"	VARCHAR(256),
	"status"	VARCHAR(256),
	"b_s"	VARCHAR(256),
	"price"	FLOAT,
	"qty"	INTEGER,
	"nkd"	FLOAT,
	"rest"	INTEGER,
	"rest_nkd"	FLOAT,
	"portfolio"	VARCHAR(256),
	"ts_time"	TIMESTAMP,
	"comments"	VARCHAR(256),
	"place_code"	VARCHAR(256),
	"p_code"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"last_date"	TIMESTAMP,
	"stop_price"	FLOAT,
	"price_curr"	VARCHAR(256),
	"non_exch"	VARCHAR(256),
	"blank"	VARCHAR(256),
	"actv_grow_price"	FLOAT,
	"actv_down_price"	FLOAT,
	"actv_time"	TIMESTAMP,
	"actv_kld_order_no"	INTEGER,
	"actv_stld_order_no"	INTEGER,
	"drop_grow_price"	FLOAT,
	"drop_down_price"	FLOAT,
	"drop_time"	TIMESTAMP,
	"drop_kld_order_no"	INTEGER,
	"drop_stld_order_no"	INTEGER,
	"updt_grow_price"	FLOAT,
	"updt_down_price"	FLOAT,
	"updt_time"	TIMESTAMP,
	"updt_kld_order_no"	INTEGER,
	"updt_stld_order_no"	INTEGER,
	"updt_new_price"	FLOAT,
	"fee"	FLOAT,
	"rest_fee"	FLOAT,
	"volume"	FLOAT,
	"net_value"	FLOAT,
	"firm_code"	VARCHAR(256),
	"cupon_rate"	VARCHAR(256),
	"trailing_level"	FLOAT,
	"trailing_slippage"	FLOAT,
	"instr"	VARCHAR(256),
	"treaty"	INTEGER,
	"sys_name"	VARCHAR(256),
	"limits_check"	VARCHAR(256),
	"init_price"	FLOAT,
	"ord_type"	VARCHAR(256),
	"yield"	FLOAT,
	"repo_days"	INTEGER,
	"repo_ammount"	FLOAT,
	"repo_discount"	FLOAT,
	"repo_ts_no"	VARCHAR(256),
	"repo_low_discount"	FLOAT,
	"repo_high_discount"	FLOAT,
	"contragent_acc_code"	VARCHAR(256),

	PRIMARY KEY ("ord_no")
) ;

CREATE TABLE IF NOT EXISTS AD_DOC_TEMPLATES (
	"doc_id"	VARCHAR(64)	NOT NULL,
	"data"	VARCHAR(256),
	"i_last_update"	INTEGER,

	PRIMARY KEY ("doc_id")
) ;

CREATE TABLE IF NOT EXISTS AD_TRADE_PLACES (
	"place_code"	VARCHAR(64)	NOT NULL,
	"curr_code"	VARCHAR(256),
	"place_name"	VARCHAR(256),
	"info_price"	FLOAT,
	"status"	VARCHAR(256),
	"last_session_date"	TIMESTAMP,
	"t_acc_prefix"	VARCHAR(256),
	"prev_session_date"	TIMESTAMP,
	"ex_code"	VARCHAR(256),
	"depo_code"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"allow_trade"	VARCHAR(256),
	"vendor_code"	VARCHAR(256),
	"dp_name"	VARCHAR(256),
	"allow_market_orders"	VARCHAR(256),
	"allow_nomarket_orders"	VARCHAR(256),
	"balance_place_code"	VARCHAR(256),
	"balance_place_name"	VARCHAR(256),
	"market_name"	VARCHAR(256),
	"allow_rpc"	VARCHAR(256),
	"allow_rpc_auction"	VARCHAR(256),
	"allow_auction"	VARCHAR(256),
	"quote_place_code"	VARCHAR(256),
	"allow_all_trades"	VARCHAR(256),

	PRIMARY KEY ("place_code")
) ;

CREATE TABLE IF NOT EXISTS AD_TRADES (
	"trd_no"	INTEGER	NOT NULL,
	"ord_no"	INTEGER,
	"acc_code"	VARCHAR(256),
	"ts_no"	VARCHAR(256),
	"trd_type_code"	VARCHAR(256),
	"b_s"	VARCHAR(256),
	"qty"	INTEGER,
	"price"	FLOAT,
	"ts_tax"	FLOAT,
	"nkd"	FLOAT,
	"ts_time"	TIMESTAMP,
	"comments"	VARCHAR(256),
	"portfolio"	VARCHAR(256),
	"p_code"	VARCHAR(256),
	"place_code"	VARCHAR(256),
	"settlement_date"	TIMESTAMP,
	"settlemented"	VARCHAR(256),
	"bank_tax"	FLOAT,
	"depo_tax"	FLOAT,
	"i_last_update"	INTEGER,
	"price_curr"	VARCHAR(256),
	"non_exch"	VARCHAR(256),
	"comiss"	FLOAT,
	"fee"	FLOAT,
	"net_value"	FLOAT,
	"volume"	FLOAT,
	"pl"	FLOAT,
	"treaty"	INTEGER,
	"paper_no"	INTEGER,
	"firm_code"	VARCHAR(256),
	"money_rest"	FLOAT,
	"paper_rest"	INTEGER,

	PRIMARY KEY ("trd_no")
) ;

CREATE TABLE IF NOT EXISTS AD_TRADE_TYPES (
	"trd_type_code"	VARCHAR(64)	NOT NULL,
	"trd_type_describtion"	VARCHAR(256),
	"i_last_update"	INTEGER,
	"trd_type_num_code"	INTEGER,

	PRIMARY KEY ("trd_type_code")
) ;

CREATE TABLE IF NOT EXISTS AD_HISTORICAL_QUOTES (
	"paper_no"	INTEGER	NOT NULL,
	"timeframe"	INTEGER	NOT NULL,
	"quote_timestamp"	TIMESTAMP	NOT NULL,
	"open"	FLOAT,
	"high"	FLOAT,
	"low"	FLOAT,
	"close"	FLOAT,
	"volume"	DOUBLE,

	PRIMARY KEY ("paper_no", "timeframe", "quote_timestamp")
) ;

CREATE TABLE IF NOT EXISTS AD_ARCHIVE_PAPERS (
	"paper_no"	INTEGER	NOT NULL,
	"p_code"	VARCHAR(256),
	"ts_p_code"	VARCHAR(256),
	"place_code"	VARCHAR(256),
	"place_name"	VARCHAR(256),
	"unused"	INTEGER,
	"expired"	VARCHAR(256),
	"board_code"	VARCHAR(256),
	"at_code"	VARCHAR(256),
	"mat_date"	TIMESTAMP,

	PRIMARY KEY ("paper_no")
) ;

CREATE TABLE IF NOT EXISTS AD_CONTINUOUS_QUOTES (
	"p_code"	VARCHAR(64)	NOT NULL,
	"paper_no"	INTEGER	NOT NULL,

	UNIQUE ("paper_no"),
	PRIMARY KEY ("p_code", "paper_no")
) ;
