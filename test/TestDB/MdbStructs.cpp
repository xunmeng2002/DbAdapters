#include "MdbStructs.h"
#include <Spark/TemplateLib/TemplateLib.h>
#include <cstdio>

using namespace Spark;
using namespace DbAdapters;
namespace Mdb
{
	thread_local char t_MdbDataStringBuffer[10240];

	TradingDay* TradingDay::Allocate()
	{
		return ObjectPool<TradingDay>::GetInstance().Allocate();
	}
	void TradingDay::Deallocate()
	{
		ObjectPool<TradingDay>::GetInstance().Deallocate(this);
	}
	const char* TradingDay::GetDebugString() const
	{
		std::snprintf(t_MdbDataStringBuffer, sizeof(t_MdbDataStringBuffer),
			"TradingDay:PK:[%d], CurrTradingDay:[%s], PreTradingDay:[%s]",
			PK, CurrTradingDay, PreTradingDay);
		return t_MdbDataStringBuffer;
	}

	Exchange* Exchange::Allocate()
	{
		return ObjectPool<Exchange>::GetInstance().Allocate();
	}
	void Exchange::Deallocate()
	{
		ObjectPool<Exchange>::GetInstance().Deallocate(this);
	}
	const char* Exchange::GetDebugString() const
	{
		std::snprintf(t_MdbDataStringBuffer, sizeof(t_MdbDataStringBuffer),
			"Exchange:ExchangeId:[%s], ExchangeName:[%s]",
			ExchangeId, ExchangeName);
		return t_MdbDataStringBuffer;
	}

	Account* Account::Allocate()
	{
		return ObjectPool<Account>::GetInstance().Allocate();
	}
	void Account::Deallocate()
	{
		ObjectPool<Account>::GetInstance().Deallocate(this);
	}
	const char* Account::GetDebugString() const
	{
		std::snprintf(t_MdbDataStringBuffer, sizeof(t_MdbDataStringBuffer),
			"Account:AccountId:[%s], AccountName:[%s], AccountType:[%d], AccountStatus:[%d], "
			"Password:[%s], TradeGroupId:[%d], RiskGroupId:[%d], CommissionGroupId:[%d]",
			AccountId, AccountName, static_cast<int>(AccountType), static_cast<int>(AccountStatus),
			Password, TradeGroupId, RiskGroupId, CommissionGroupId);
		return t_MdbDataStringBuffer;
	}

static const FieldDescriptor TradingDayFields[] = {
	{"PK",             FieldType::Int32, offsetof(TradingDay, PK),             0},
	{"CurrTradingDay", FieldType::Char,  offsetof(TradingDay, CurrTradingDay), sizeof(TradingDay::CurrTradingDay)},
	{"PreTradingDay",  FieldType::Char,  offsetof(TradingDay, PreTradingDay),  sizeof(TradingDay::PreTradingDay)},
};
static const int TradingDayPKIndices[] = {0};

const TableSchema& TradingDay::GetSchema()
{
	static const TableSchema schema = {"t_TradingDay", TradingDayFields, 3, TradingDayPKIndices, 1, nullptr, 0};
	return schema;
}

static const FieldDescriptor ExchangeFields[] = {
	{"ExchangeId",   FieldType::Char, offsetof(Exchange, ExchangeId),   sizeof(Exchange::ExchangeId)},
	{"ExchangeName", FieldType::Char, offsetof(Exchange, ExchangeName), sizeof(Exchange::ExchangeName)},
};
static const int ExchangePKIndices[] = {0};

const TableSchema& Exchange::GetSchema()
{
	static const TableSchema schema = {"t_Exchange", ExchangeFields, 2, ExchangePKIndices, 1, nullptr, 0};
	return schema;
}

static const FieldDescriptor AccountFields[] = {
	{"AccountId",         FieldType::Char,  offsetof(Account, AccountId),         sizeof(Account::AccountId)},
	{"AccountName",       FieldType::Char,  offsetof(Account, AccountName),       sizeof(Account::AccountName)},
	{"AccountType",       FieldType::Int32, offsetof(Account, AccountType),       0},
	{"AccountStatus",     FieldType::Int32, offsetof(Account, AccountStatus),     0},
	{"Password",          FieldType::Char,  offsetof(Account, Password),          sizeof(Account::Password)},
	{"TradeGroupId",      FieldType::Int32, offsetof(Account, TradeGroupId),      0},
	{"RiskGroupId",       FieldType::Int32, offsetof(Account, RiskGroupId),       0},
	{"CommissionGroupId", FieldType::Int32, offsetof(Account, CommissionGroupId), 0},
};
static const int AccountPKIndices[] = {0};

const TableSchema& Account::GetSchema()
{
	static const TableSchema schema = {"t_Account", AccountFields, 8, AccountPKIndices, 1, nullptr, 0};
	return schema;
}
}
