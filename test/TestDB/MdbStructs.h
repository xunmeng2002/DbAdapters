#pragma once
#include <DbAdapters/DbInterface/Schema.h>
#include <stdint.h>

using DbAdapters::TableSchema;

namespace Mdb
{
using Int32Type = int32_t;
using DateType = char[16];
using ExchangeIdType = char[8];
using ExchangeNameType = char[64];
using AccountIdType = char[32];
using AccountNameType = char[64];
using PasswordType = char[64];
using GroupIdType = int32_t;

enum class AccountTypeType : int32_t
{
	Primary = 0,
	Sub = 1,
};

enum class AccountStatusType : int32_t
{
	Normal = 0,
	Forbidden = 1,
};

class TradingDay
{
public:
	Int32Type PK;
	DateType CurrTradingDay;
	DateType PreTradingDay;

	static TradingDay* Allocate();
	void Deallocate();
	const char* GetDebugString() const;
	static const TableSchema& GetSchema();
};

class Exchange
{
public:
	ExchangeIdType ExchangeId;
	ExchangeNameType ExchangeName;

	static Exchange* Allocate();
	void Deallocate();
	const char* GetDebugString() const;
	static const TableSchema& GetSchema();
};

class Account
{
public:
	AccountIdType AccountId;
	AccountNameType AccountName;
	AccountTypeType AccountType;
	AccountStatusType AccountStatus;
	PasswordType Password;
	GroupIdType TradeGroupId;
	GroupIdType RiskGroupId;
	GroupIdType CommissionGroupId;

	static Account* Allocate();
	void Deallocate();
	const char* GetDebugString() const;
	static const TableSchema& GetSchema();
};
}
