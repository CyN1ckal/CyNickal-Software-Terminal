// Copyright 2026 CyNickal Software LLC
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0

#include "data/StatementSheet.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace terminal {
namespace {

struct LineName
{
    std::string_view key;
    std::string_view label;
};

// MBoum income, balance, and cash-flow module keys, ordered for binary search.
// Industry templates keep a parenthetical so two slugs on one grid do not share a name.
constexpr LineName kLineNames[] = {
    {.key = "accountsPayable", .label = "Accounts Payable"},
    {.key = "accountsPayableBank", .label = "Accounts Payable (Banks)"},
    {.key = "accountsPayableFin", .label = "Accounts Payable (Finance)"},
    {.key = "accountsPayableIns", .label = "Accounts Payable (Insurance)"},
    {.key = "accountsPayableRE", .label = "Accounts Payable (Real Estate)"},
    {.key = "accountsPayableUti", .label = "Accounts Payable (Utilities)"},
    {.key = "accountsReceivable", .label = "Accounts Receivable"},
    {.key = "accountsReceivableCM", .label = "Accounts Receivable (Capital Markets)"},
    {.key = "accountsReceivableUti", .label = "Accounts Receivable (Utilities)"},
    {.key = "accruedExpenses", .label = "Accrued Expenses"},
    {.key = "accruedExpensesBank", .label = "Accrued Expenses (Banks)"},
    {.key = "accruedExpensesRE", .label = "Accrued Expenses (Real Estate)"},
    {.key = "accruedExpensesUti", .label = "Accrued Expenses (Utilities)"},
    {.key = "accruedInterestPayable", .label = "Accrued Interest Payable"},
    {.key = "accruedInterestReceivableBank", .label = "Accrued Interest Receivable (Banks)"},
    {.key = "accruedInterestReceivables", .label = "Accrued Interest Receivable"},
    {.key = "acquisitionRealEstateAssets", .label = "Acquisition of Real Estate"},
    {.key = "additionalPaidInCapital", .label = "Additional Paid-in Capital"},
    {.key = "allowanceForLoanLosses", .label = "Allowance for Loan Losses"},
    {.key = "assetWritedownBankCF", .label = "Asset Writedowns (Banks)"},
    {.key = "assetWritedownCF", .label = "Asset Writedowns"},
    {.key = "assetWritedownCFRE", .label = "Asset Writedowns (Real Estate)"},
    {.key = "assetWritedownCFUti", .label = "Asset Writedowns (Utilities)"},
    {.key = "assets", .label = "Total Assets"},
    {.key = "assetsc", .label = "Total Current Assets"},
    {.key = "buildings", .label = "Buildings"},
    {.key = "bvps", .label = "Book Value per Share"},
    {.key = "capex", .label = "Capital Expenditures"},
    {.key = "capexIns", .label = "Capital Expenditures (Insurance)"},
    {.key = "capexUti", .label = "Capital Expenditures (Utilities)"},
    {.key = "capitalLeases", .label = "Capital Lease Obligations"},
    {.key = "cashAcquisition", .label = "Acquisitions, Net of Cash"},
    {.key = "cashInterestPaid", .label = "Cash Interest Paid"},
    {.key = "cashTaxesPaid", .label = "Cash Taxes Paid"},
    {.key = "cashneq", .label = "Cash and Equivalents"},
    {.key = "changeAP", .label = "Change in Accounts Payable"},
    {.key = "changeAR", .label = "Change in Accounts Receivable"},
    {.key = "changeAccountsPayableIns", .label = "Change in Accounts Payable (Insurance)"},
    {.key = "changeAccountsPayableRE", .label = "Change in Accounts Payable (Real Estate)"},
    {.key = "changeAccountsReceivableIns", .label = "Change in Accounts Receivable (Insurance)"},
    {.key = "changeAccountsReceivableRE", .label = "Change in Accounts Receivable (Real Estate)"},
    {.key = "changeDeferredTax", .label = "Change in Deferred Taxes"},
    {.key = "changeInDepositAccount", .label = "Change in Deposits"},
    {.key = "changeInTradingAssets", .label = "Change in Trading Assets"},
    {.key = "changeIncomeTax", .label = "Change in Income Taxes"},
    {.key = "changeInsuranceReservesLiabilities", .label = "Change in Insurance Reserves"},
    {.key = "changeInventory", .label = "Change in Inventory"},
    {.key = "changeOtherNetOperAssets", .label = "Change in Other Operating Assets"},
    {.key = "changeOtherNetOperAssetsIns", .label = "Change in Other Operating Assets (Insurance)"},
    {.key = "changeOtherNetOperAssetsRE", .label = "Change in Other Operating Assets (Real Estate)"},
    {.key = "changeOtherNetOperatingAssetsBank", .label = "Change in Other Operating Assets (Banks)"},
    {.key = "changeUnearnedRev", .label = "Change in Unearned Revenue"},
    {.key = "changeWorkingCapital", .label = "Change in Working Capital"},
    {.key = "commonDividendCF", .label = "Common Dividends"},
    {.key = "commonIssued", .label = "Common Stock Issued"},
    {.key = "commonIssuedBank", .label = "Common Stock Issued (Banks)"},
    {.key = "commonIssuedIns", .label = "Common Stock Issued (Insurance)"},
    {.key = "commonPreferredDividendCF", .label = "Common and Preferred Dividends"},
    {.key = "commonRepurchased", .label = "Common Stock Repurchased"},
    {.key = "commonRepurchasedBank", .label = "Common Stock Repurchased (Banks)"},
    {.key = "commonRepurchasedIns", .label = "Common Stock Repurchased (Insurance)"},
    {.key = "commonStock", .label = "Common Stock"},
    {.key = "constructionInProgress", .label = "Construction in Progress"},
    {.key = "cor", .label = "Cost of Revenue"},
    {.key = "currentCapLeases", .label = "Current Capital Lease Obligations"},
    {.key = "currentIncomeTaxesPayable", .label = "Income Taxes Payable"},
    {.key = "currentLiabilities", .label = "Total Current Liabilities"},
    {.key = "currentPortDebt", .label = "Current Portion of Debt"},
    {.key = "currentPortDebtBank", .label = "Current Portion of Debt (Banks)"},
    {.key = "currentPortLongTermDebtRE", .label = "Current Portion of Long-Term Debt (Real Estate)"},
    {.key = "currentPortLongTermDebtUti", .label = "Current Portion of Long-Term Debt (Utilities)"},
    {.key = "currentUnearnedRevenue", .label = "Unearned Revenue, Current"},
    {.key = "currentUnearnedRevenueUti", .label = "Unearned Revenue, Current (Utilities)"},
    {.key = "debt", .label = "Total Debt"},
    {.key = "debtIssuedLongTerm", .label = "Long-Term Debt Issued"},
    {.key = "debtIssuedShortTerm", .label = "Short-Term Debt Issued"},
    {.key = "debtIssuedTotal", .label = "Debt Issued"},
    {.key = "debtRepaidLongTerm", .label = "Long-Term Debt Repaid"},
    {.key = "debtRepaidShortTerm", .label = "Short-Term Debt Repaid"},
    {.key = "debtRepaidTotal", .label = "Debt Repaid"},
    {.key = "debtc", .label = "Current Debt"},
    {.key = "debtnc", .label = "Long-Term Debt"},
    {.key = "defChargesLtUti", .label = "Long-Term Deferred Charges (Utilities)"},
    {.key = "deferredLongTermCharges", .label = "Long-Term Deferred Charges"},
    {.key = "deferredLongTermChargesRE", .label = "Long-Term Deferred Charges (Real Estate)"},
    {.key = "deferredPolicyAcquisitionCost", .label = "Deferred Policy Acquisition Costs"},
    {.key = "deferredTaxLiabilitiesBank", .label = "Deferred Tax Liabilities (Banks)"},
    {.key = "defferedTaxAssets", .label = "Deferred Tax Assets"},
    {.key = "defferedTaxAssetsBank", .label = "Deferred Tax Assets (Banks)"},
    {.key = "defferedTaxAssetsUti", .label = "Deferred Tax Assets (Utilities)"},
    {.key = "defferedTaxLiabilitiesIns", .label = "Deferred Tax Liabilities (Insurance)"},
    {.key = "depamorCFUti", .label = "Depreciation (Utilities)"},
    {.key = "distrExcessEarn", .label = "Distributions in Excess of Earnings"},
    {.key = "divestitures", .label = "Divestitures"},
    {.key = "ebit", .label = "EBIT"},
    {.key = "ebitda", .label = "EBITDA"},
    {.key = "eps", .label = "Basic EPS"},
    {.key = "epsdil", .label = "Diluted EPS"},
    {.key = "equity", .label = "Total Equity"},
    {.key = "exchangeRateAdjustments", .label = "Effect of Exchange Rates on Cash"},
    {.key = "fcf", .label = "Free Cash Flow"},
    {.key = "fcfAfterLeases", .label = "Free Cash Flow After Leases"},
    {.key = "fcfMargin", .label = "Free Cash Flow Margin"},
    {.key = "fcfps", .label = "Free Cash Flow per Share"},
    {.key = "fhlbDebt", .label = "Federal Home Loan Bank Debt"},
    {.key = "finDivAssetsCurrent", .label = "Financial Division Assets, Current"},
    {.key = "finDivDebtCurrent", .label = "Financial Division Debt, Current"},
    {.key = "finDivDebtLT", .label = "Financial Division Debt, Long-Term"},
    {.key = "finDivLiabCurrent", .label = "Financial Division Liabilities, Current"},
    {.key = "finDivLiabLT", .label = "Financial Division Liabilities, Long-Term"},
    {.key = "finDivLoansCurrent", .label = "Financial Division Loans, Current"},
    {.key = "finDivLoansLT", .label = "Financial Division Loans, Long-Term"},
    {.key = "fiscalQuarter", .label = "Fiscal Quarter"},
    {.key = "fiscalYear", .label = "Fiscal Year"},
    {.key = "gainAssetsCF", .label = "Gain on Sale of Assets"},
    {.key = "gainAssetsCFIns", .label = "Gain on Sale of Assets (Insurance)"},
    {.key = "gainAssetsCFRE", .label = "Gain on Sale of Assets (Real Estate)"},
    {.key = "gainAssetsCFUti", .label = "Gain on Sale of Assets (Utilities)"},
    {.key = "gainInvestmentsCF", .label = "Gain on Investments"},
    {.key = "gainLossSaleOfAssetsBank", .label = "Gain or Loss on Sale of Assets (Banks)"},
    {.key = "gainLossSaleOfInvestmentsBank", .label = "Gain or Loss on Investments (Banks)"},
    {.key = "goodwill", .label = "Goodwill"},
    {.key = "gp", .label = "Gross Profit"},
    {.key = "grossLoans", .label = "Gross Loans"},
    {.key = "gwIntangAmortCFUti", .label = "Goodwill and Intangible Amortization (Utilities)"},
    {.key = "incomeLossEquityInvestments", .label = "Equity-Method Investment Income"},
    {.key = "institutionalDeposits", .label = "Institutional Deposits"},
    {.key = "insuranceAnnuityLiabilities", .label = "Insurance and Annuity Liabilities"},
    {.key = "interestBearingDeposits", .label = "Interest-Bearing Deposits"},
    {.key = "inventory", .label = "Inventory"},
    {.key = "inventoryUti", .label = "Inventory (Utilities)"},
    {.key = "investInSecurities", .label = "Investment in Securities"},
    {.key = "investInSecuritiesBank", .label = "Investment in Securities (Banks)"},
    {.key = "investInSecuritiesRE", .label = "Investment in Securities (Real Estate)"},
    {.key = "investLoansCF", .label = "Investment in Loans"},
    {.key = "investmentDebtSecurities", .label = "Investment Debt Securities"},
    {.key = "investmentEquityPreferred", .label = "Equity and Preferred Investments"},
    {.key = "investmentSecurities", .label = "Investment Securities"},
    {.key = "investmentsc", .label = "Current Investments"},
    {.key = "investmentsnc", .label = "Long-Term Investments"},
    {.key = "land", .label = "Land"},
    {.key = "leaseholdImprovements", .label = "Leasehold Improvements"},
    {.key = "leveredFCF", .label = "Levered Free Cash Flow"},
    {.key = "liabilities", .label = "Total Liabilities"},
    {.key = "liabilitiesBank", .label = "Total Liabilities (Banks)"},
    {.key = "liabilitiesequity", .label = "Total Liabilities and Equity"},
    {.key = "loansCF", .label = "Net Loans Issued"},
    {.key = "loansHeldForSale", .label = "Loans Held for Sale"},
    {.key = "loansLeaseReceivables", .label = "Loans and Lease Receivables"},
    {.key = "loansReceivableCurrent", .label = "Current Loans Receivable"},
    {.key = "loansReceivableLtUti", .label = "Long-Term Loans Receivable (Utilities)"},
    {.key = "longTermAccountsReceivable", .label = "Long-Term Accounts Receivable"},
    {.key = "longTermDebtBank", .label = "Long-Term Debt (Banks)"},
    {.key = "longTermDebtRE", .label = "Long-Term Debt (Real Estate)"},
    {.key = "longTermDebtUti", .label = "Long-Term Debt (Utilities)"},
    {.key = "longTermDeferredTaxLiabilities", .label = "Long-Term Deferred Tax Liabilities"},
    {.key = "longTermDeferredTaxLiabilitiesRE", .label = "Long-Term Deferred Tax Liabilities (Real Estate)"},
    {.key = "longTermDeferredTaxLiabilitiesUti", .label = "Long-Term Deferred Tax Liabilities (Utilities)"},
    {.key = "longTermInvestmentsCM", .label = "Long-Term Investments (Capital Markets)"},
    {.key = "longTermInvestmentsRE", .label = "Long-Term Investments (Real Estate)"},
    {.key = "longTermInvestmentsUti", .label = "Long-Term Investments (Utilities)"},
    {.key = "longTermUnearnedRevenue", .label = "Long-Term Unearned Revenue"},
    {.key = "machinery", .label = "Machinery and Equipment"},
    {.key = "minorityInterestBS", .label = "Minority Interest"},
    {.key = "minorityInterestBank", .label = "Minority Interest (Banks)"},
    {.key = "miscCashFlowAdjustments", .label = "Other Non-Cash Adjustments"},
    {.key = "mortgageBackedSecurities", .label = "Mortgage-Backed Securities"},
    {.key = "ncf", .label = "Net Change in Cash"},
    {.key = "ncff", .label = "Financing Cash Flow"},
    {.key = "ncfi", .label = "Investing Cash Flow"},
    {.key = "ncfo", .label = "Operating Cash Flow"},
    {.key = "netCashFromDiscontinuedOperations", .label = "Cash from Discontinued Operations"},
    {.key = "netDebtIssued", .label = "Net Debt Issued"},
    {.key = "netDebtIssuedBank", .label = "Net Debt Issued (Banks)"},
    {.key = "netDebtIssuedIns", .label = "Net Debt Issued (Insurance)"},
    {.key = "netIncomeCF", .label = "Net Income (Cash Flow)"},
    {.key = "netLoans", .label = "Net Loans"},
    {.key = "netNuclearFuel", .label = "Nuclear Fuel, Net"},
    {.key = "netPPE", .label = "Net Property, Plant and Equipment"},
    {.key = "netSaleAcquisitionRealEstateAssets", .label = "Net Acquisition of Real Estate"},
    {.key = "netcash", .label = "Net Cash"},
    {.key = "netcashpershare", .label = "Net Cash per Share"},
    {.key = "netinc", .label = "Net Income"},
    {.key = "netinccmn", .label = "Net Income to Common"},
    {.key = "nonInterestBearingDeposits", .label = "Noninterest-Bearing Deposits"},
    {.key = "nukeCF", .label = "Nuclear Decommissioning"},
    {.key = "nukeContr", .label = "Nuclear Decommissioning Contributions"},
    {.key = "operatingLeasePayments", .label = "Operating Lease Payments"},
    {.key = "opex", .label = "Operating Expenses"},
    {.key = "opinc", .label = "Operating Income"},
    {.key = "orderBacklog", .label = "Order Backlog"},
    {.key = "otherAdjustGrossLoans", .label = "Other Loan Loss Adjustments"},
    {.key = "otherAmortization", .label = "Other Amortization"},
    {.key = "otherAmortizationBank", .label = "Other Amortization (Banks)"},
    {.key = "otherAmortizationUti", .label = "Other Amortization (Utilities)"},
    {.key = "otherCurrentAssetsBank", .label = "Other Current Assets (Banks)"},
    {.key = "otherCurrentAssetsIns", .label = "Other Current Assets (Insurance)"},
    {.key = "otherCurrentAssetsRE", .label = "Other Current Assets (Real Estate)"},
    {.key = "otherCurrentAssetsUti", .label = "Other Current Assets (Utilities)"},
    {.key = "otherCurrentLiabilities", .label = "Other Current Liabilities"},
    {.key = "otherCurrentLiabilitiesBank", .label = "Other Current Liabilities (Banks)"},
    {.key = "otherCurrentLiabilitiesIns", .label = "Other Current Liabilities (Insurance)"},
    {.key = "otherCurrentLiabilitiesRE", .label = "Other Current Liabilities (Real Estate)"},
    {.key = "otherCurrentLiabilitiesUti", .label = "Other Current Liabilities (Utilities)"},
    {.key = "otherEquity", .label = "Other Equity"},
    {.key = "otherEquityBank", .label = "Other Equity (Banks)"},
    {.key = "otherEquityRE", .label = "Other Equity (Real Estate)"},
    {.key = "otherEquityUti", .label = "Other Equity (Utilities)"},
    {.key = "otherFinancingActivitiesBank", .label = "Other Financing Activities (Banks)"},
    {.key = "otherFinancingActivitiesIns", .label = "Other Financing Activities (Insurance)"},
    {.key = "otherFinancingActivitiesRE", .label = "Other Financing Activities (Real Estate)"},
    {.key = "otherIntanBank", .label = "Other Intangible Assets (Banks)"},
    {.key = "otherIntangibles", .label = "Other Intangible Assets"},
    {.key = "otherIntangiblesIns", .label = "Other Intangible Assets (Insurance)"},
    {.key = "otherIntangiblesUti", .label = "Other Intangible Assets (Utilities)"},
    {.key = "otherInvestActSupplBank", .label = "Supplemental Investing Activities (Banks)"},
    {.key = "otherInvestingActivitiesRE", .label = "Other Investing Activities (Real Estate)"},
    {.key = "otherInvestingActivitiesUti", .label = "Other Investing Activities (Utilities)"},
    {.key = "otherInvestments", .label = "Other Investments"},
    {.key = "otherLiabLtFin", .label = "Other Long-Term Liabilities (Finance)"},
    {.key = "otherLongTermAssetsBank", .label = "Other Long-Term Assets (Banks)"},
    {.key = "otherLongTermAssetsCM", .label = "Other Long-Term Assets (Capital Markets)"},
    {.key = "otherLongTermAssetsIns", .label = "Other Long-Term Assets (Insurance)"},
    {.key = "otherLongTermAssetsRE", .label = "Other Long-Term Assets (Real Estate)"},
    {.key = "otherLongTermAssetsUti", .label = "Other Long-Term Assets (Utilities)"},
    {.key = "otherLongTermLiabilitiesBank", .label = "Other Long-Term Liabilities (Banks)"},
    {.key = "otherLongTermLiabilitiesIns", .label = "Other Long-Term Liabilities (Insurance)"},
    {.key = "otherLongTermLiabilitiesRE", .label = "Other Long-Term Liabilities (Real Estate)"},
    {.key = "otherLongTermLiabilitiesUti", .label = "Other Long-Term Liabilities (Utilities)"},
    {.key = "otherOperatingActivitiesBank", .label = "Other Operating Activities (Banks)"},
    {.key = "otherOperatingActivitiesIns", .label = "Other Operating Activities (Insurance)"},
    {.key = "otherOperatingActivitiesRE", .label = "Other Operating Activities (Real Estate)"},
    {.key = "otherRealEstate", .label = "Other Real Estate Owned"},
    {.key = "otherReceivables", .label = "Other Receivables"},
    {.key = "otherReceivablesIns", .label = "Other Receivables (Insurance)"},
    {.key = "othercurrent", .label = "Other Current Assets"},
    {.key = "otherfinancing", .label = "Other Financing Activities"},
    {.key = "otherinvesting", .label = "Other Investing Activities"},
    {.key = "otherliabilitiesnoncurrent", .label = "Other Long-Term Liabilities"},
    {.key = "othernoncurrent", .label = "Other Long-Term Assets"},
    {.key = "otheroperating", .label = "Other Operating Activities"},
    {.key = "pensionRetirementBenefits", .label = "Pension and Other Retirement Benefits"},
    {.key = "policyLoans", .label = "Policy Loans"},
    {.key = "preferredConvertible", .label = "Convertible Preferred Stock"},
    {.key = "preferredDividendCF", .label = "Preferred Dividends"},
    {.key = "preferredIssuedBank", .label = "Preferred Stock Issued (Banks)"},
    {.key = "preferredIssuedIns", .label = "Preferred Stock Issued (Insurance)"},
    {.key = "preferredOtherBank", .label = "Other Preferred Stock (Banks)"},
    {.key = "preferredRedeemable", .label = "Redeemable Preferred Stock"},
    {.key = "preferredRepurchased", .label = "Preferred Stock Repurchased"},
    {.key = "preferredRepurchasedBank", .label = "Preferred Stock Repurchased (Banks)"},
    {.key = "preferredRepurchasedIns", .label = "Preferred Stock Repurchased (Insurance)"},
    {.key = "prepaidExpenses", .label = "Prepaid Expenses"},
    {.key = "provisionForCreditLosses", .label = "Provision for Credit Losses"},
    {.key = "provisionWriteoffBadDebtsCF", .label = "Bad Debt Expense"},
    {.key = "receivables", .label = "Total Receivables"},
    {.key = "regulatoryAssets", .label = "Regulatory Assets"},
    {.key = "reinsurancePayable", .label = "Reinsurance Payable"},
    {.key = "reinsuranceRecoverable", .label = "Reinsurance Recoverable"},
    {.key = "reinsuranceRecoverableCF", .label = "Change in Reinsurance Recoverable"},
    {.key = "restrictedCash", .label = "Restricted Cash"},
    {.key = "restructureCF", .label = "Restructuring"},
    {.key = "retearn", .label = "Retained Earnings"},
    {.key = "revenue", .label = "Revenue"},
    {.key = "saleOfPropertyPlantAndEquipment", .label = "Sale of Property, Plant and Equipment"},
    {.key = "saleOfPropertyPlantAndEquipmentBank", .label = "Sale of Property, Plant and Equipment (Banks)"},
    {.key = "salePurchaseIntangibles", .label = "Purchase or Sale of Intangibles"},
    {.key = "saleRealEstateAssets", .label = "Sale of Real Estate"},
    {.key = "sbcomp", .label = "Stock-Based Compensation"},
    {.key = "separateAccountAssets", .label = "Separate Account Assets"},
    {.key = "separateAccountLiability", .label = "Separate Account Liabilities"},
    {.key = "sharesOutFilingDate", .label = "Shares Outstanding at Filing Date"},
    {.key = "sharesOutTotalCommon", .label = "Common Shares Outstanding"},
    {.key = "shortTermDebtBank", .label = "Short-Term Debt (Banks)"},
    {.key = "shortTermDebtUti", .label = "Short-Term Debt (Utilities)"},
    {.key = "tangibleBookValue", .label = "Tangible Book Value"},
    {.key = "tangibleBookValuePerShare", .label = "Tangible Book Value per Share"},
    {.key = "totalCommonEquity", .label = "Common Equity"},
    {.key = "totalDebtRepaidIns", .label = "Debt Repaid (Insurance)"},
    {.key = "totalDepAmorCF", .label = "Depreciation and Amortization"},
    {.key = "totalDepAmorCFBank", .label = "Depreciation and Amortization (Banks)"},
    {.key = "totalDepAmorCFUti", .label = "Depreciation and Amortization (Utilities)"},
    {.key = "totalDeposits", .label = "Total Deposits"},
    {.key = "totalDividendPaidCF", .label = "Dividends Paid"},
    {.key = "totalInvestment", .label = "Total Investments"},
    {.key = "totalLiabilitiesIns", .label = "Total Liabilities (Insurance)"},
    {.key = "totalLiabilitiesRE", .label = "Total Liabilities (Real Estate)"},
    {.key = "totalPreferredEquity", .label = "Preferred Equity"},
    {.key = "totalRealEstateAssets", .label = "Total Real Estate Assets"},
    {.key = "totalcash", .label = "Cash and Short-Term Investments"},
    {.key = "tradingAssetSecurities", .label = "Trading Asset Securities"},
    {.key = "treasuryStock", .label = "Treasury Stock"},
    {.key = "trustPref", .label = "Trust Preferred Securities"},
    {.key = "unearnedPremiums", .label = "Unearned Premiums"},
    {.key = "unleveredFCF", .label = "Unlevered Free Cash Flow"},
    {.key = "unpaidClaims", .label = "Unpaid Claims"},
    {.key = "workingcapital", .label = "Working Capital"},
};

constexpr bool lineNamesAreOrdered()
{
    for (std::size_t index = 1; index < std::size(kLineNames); ++index)
    {
        if (!(kLineNames[index - 1].key < kLineNames[index].key))
        {
            return false;
        }
    }
    return true;
}

static_assert(lineNamesAreOrdered());

[[nodiscard]] bool isAsciiUpper(unsigned char ch)
{
    return ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z');
}

[[nodiscard]] bool isAsciiLower(unsigned char ch)
{
    return ch >= static_cast<unsigned char>('a') && ch <= static_cast<unsigned char>('z');
}

[[nodiscard]] char toAsciiUpper(unsigned char ch)
{
    if (isAsciiLower(ch))
    {
        return static_cast<char>(ch - static_cast<unsigned char>('a') + static_cast<unsigned char>('A'));
    }
    return static_cast<char>(ch);
}

[[nodiscard]] char toAsciiLower(unsigned char ch)
{
    if (isAsciiUpper(ch))
    {
        return static_cast<char>(ch - static_cast<unsigned char>('A') + static_cast<unsigned char>('a'));
    }
    return static_cast<char>(ch);
}

[[nodiscard]] std::string titleCaseWord(std::string_view word)
{
    bool all_upper = !word.empty();
    for (const char raw : word)
    {
        if (!isAsciiUpper(static_cast<unsigned char>(raw)))
        {
            all_upper = false;
            break;
        }
    }
    if (all_upper)
    {
        return std::string{word};
    }
    std::string out;
    out.reserve(word.size());
    bool start = true;
    for (const char raw : word)
    {
        const auto ch = static_cast<unsigned char>(raw);
        if (start)
        {
            out.push_back(toAsciiUpper(ch));
            start = false;
            continue;
        }
        out.push_back(toAsciiLower(ch));
    }
    return out;
}

void appendTitledWord(std::string& out, std::string_view word)
{
    if (word.empty())
    {
        return;
    }
    if (!out.empty())
    {
        out.push_back(' ');
    }
    out += titleCaseWord(word);
}

// A capital after a lowercase letter, or an acronym before a lowercase letter, starts a word.
[[nodiscard]] std::string sanitizeLineSlug(std::string_view slug)
{
    std::string out;
    out.reserve(slug.size() + 8);
    std::size_t word_begin = 0;
    for (std::size_t index = 1; index < slug.size(); ++index)
    {
        const auto ch = static_cast<unsigned char>(slug[index]);
        if (!isAsciiUpper(ch))
        {
            continue;
        }
        const auto prev = static_cast<unsigned char>(slug[index - 1]);
        const bool next_lower =
            index + 1 < slug.size() && isAsciiLower(static_cast<unsigned char>(slug[index + 1]));
        if (isAsciiLower(prev) || (isAsciiUpper(prev) && next_lower))
        {
            appendTitledWord(out, slug.substr(word_begin, index - word_begin));
            word_begin = index;
        }
    }
    appendTitledWord(out, slug.substr(word_begin));
    return out;
}

}  // namespace

std::string statementLineLabel(std::string_view line_item)
{
    const auto* const found = std::ranges::lower_bound(kLineNames, line_item, {}, &LineName::key);
    if (found != std::ranges::end(kLineNames) && found->key == line_item)
    {
        return std::string{found->label};
    }
    return sanitizeLineSlug(line_item);
}

}  // namespace terminal
