/*
 * atm_sim.c
 * Simple ATM Simulation in C with file-based persistence.
 * Compile: gcc -o atm_sim atm_sim.c
 *
 * Features:
 * - Create account
 * - Login with account number + PIN
 * - Check balance
 * - Withdraw, deposit
 * - Transfer to another account
 * - Mini-statement (last N transactions)
 * - Change PIN
 * - File persistence in binary "accounts.dat"
 *
 * Note: This is a simple demonstration. For real systems use secure storage, hashing, validations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ACCOUNTS 200
#define NAME_LEN 64
#define MAX_TXNS 10
#define DATAFILE "accounts.dat"

typedef struct {
    char type[8];     // "DEPOSIT", "WITHDRAW", "TRANSFER"
    double amount;
    time_t ts;
    long other_acc;   // other account for transfer (0 if not)
} txn_t;

typedef struct {
    long acc_no;
    char name[NAME_LEN];
    int pin;
    double balance;
    txn_t txns[MAX_TXNS];
    int txn_count;
} account_t;

account_t accounts[MAX_ACCOUNTS];
int account_count = 0;
long next_acc_no = 100100; // starting account number

/* ---------- Persistence ---------- */
void load_accounts() {
    FILE *f = fopen(DATAFILE, "rb");
    if (!f) return;
    if (fread(&account_count, sizeof(int), 1, f) != 1) { fclose(f); return; }
    if (fread(&next_acc_no, sizeof(long), 1, f) != 1) { fclose(f); return; }
    if (fread(accounts, sizeof(account_t), account_count, f) != (size_t)account_count) {
        // partial read - ignore
    }
    fclose(f);
}

void save_accounts() {
    FILE *f = fopen(DATAFILE, "wb");
    if (!f) { perror("save_accounts fopen"); return; }
    fwrite(&account_count, sizeof(int), 1, f);
    fwrite(&next_acc_no, sizeof(long), 1, f);
    fwrite(accounts, sizeof(account_t), account_count, f);
    fclose(f);
}

/* ---------- Utilities ---------- */
account_t* find_account(long acc_no) {
    for (int i = 0; i < account_count; ++i)
        if (accounts[i].acc_no == acc_no) return &accounts[i];
    return NULL;
}

void add_txn(account_t *acc, const char *type, double amount, long other) {
    if (!acc) return;
    txn_t t;
    strncpy(t.type, type, sizeof(t.type)-1); t.type[sizeof(t.type)-1]=0;
    t.amount = amount;
    t.ts = time(NULL);
    t.other_acc = other;
    if (acc->txn_count < MAX_TXNS) {
        acc->txns[acc->txn_count++] = t;
    } else {
        // rotate: drop oldest
        for (int i = 1; i < MAX_TXNS; ++i) acc->txns[i-1] = acc->txns[i];
        acc->txns[MAX_TXNS-1] = t;
    }
}

/* ---------- UI & Logic ---------- */
void create_account() {
    if (account_count >= MAX_ACCOUNTS) {
        printf("Reached maximum account limit.\n");
        return;
    }
    account_t acc;
    printf("Enter customer name: ");
    getchar(); // consume newline if any
    fgets(acc.name, NAME_LEN, stdin);
    acc.name[strcspn(acc.name, "\n")] = 0;

    printf("Set 4-digit PIN (numbers only): ");
    if (scanf("%d", &acc.pin) != 1) { while(getchar()!='\n'); printf("Invalid input.\n"); return; }
    if (acc.pin < 0) acc.pin = -acc.pin;

    printf("Initial deposit amount: ");
    if (scanf("%lf", &acc.balance) != 1) { while(getchar()!='\n'); printf("Invalid input.\n"); return; }

    acc.acc_no = next_acc_no++;
    acc.txn_count = 0;
    add_txn(&acc, "DEPOSIT", acc.balance, 0);

    accounts[account_count++] = acc;
    save_accounts();
    printf("Account created successfully!\nAccount Number: %ld\n", acc.acc_no);
}

int login_account(account_t **out_acc) {
    long acc_no;
    int pin;
    printf("Enter account number: ");
    if (scanf("%ld", &acc_no) != 1) { while(getchar()!='\n'); printf("Invalid input.\n"); return 0; }
    printf("Enter PIN: ");
    if (scanf("%d", &pin) != 1) { while(getchar()!='\n'); printf("Invalid input.\n"); return 0; }

    account_t *acc = find_account(acc_no);
    if (!acc) { printf("Account not found.\n"); return 0; }
    if (acc->pin != pin) { printf("Incorrect PIN.\n"); return 0; }
    *out_acc = acc;
    return 1;
}

void show_mini_statement(account_t *acc) {
    printf("Mini-statement for %s (Acc: %ld)\n", acc->name, acc->acc_no);
    printf("Recent transactions (most recent last):\n");
    for (int i = 0; i < acc->txn_count; ++i) {
        char buf[64];
        struct tm *tm = localtime(&acc->txns[i].ts);
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
        if (acc->txns[i].other_acc)
            printf("%s | %s | %.2f | other acc: %ld\n", buf, acc->txns[i].type, acc->txns[i].amount, acc->txns[i].other_acc);
        else
            printf("%s | %s | %.2f\n", buf, acc->txns[i].type, acc->txns[i].amount);
    }
}

void atm_session(account_t *acc) {
    int choice;
    while (1) {
        printf("\nWelcome, %s (Acc %ld)\n", acc->name, acc->acc_no);
        printf("1) Check Balance\n2) Withdraw\n3) Deposit\n4) Transfer\n5) Mini-Statement\n6) Change PIN\n7) Logout\nChoose: ");
        if (scanf("%d", &choice) != 1) { while(getchar()!='\n'); printf("Invalid.\n"); continue; }
        if (choice == 1) {
            printf("Available balance: %.2f\n", acc->balance);
        } else if (choice == 2) {
            double amt;
            printf("Enter amount to withdraw: ");
            if (scanf("%lf", &amt) != 1) { while(getchar()!='\n'); printf("Invalid.\n"); continue; }
            if (amt <= 0) { printf("Invalid amount.\n"); continue; }
            if (amt > acc->balance) { printf("Insufficient funds.\n"); continue; }
            acc->balance -= amt;
            add_txn(acc, "WITHDRAW", amt, 0);
            save_accounts();
            printf("Withdrawn %.2f. New balance: %.2f\n", amt, acc->balance);
        } else if (choice == 3) {
            double amt;
            printf("Enter amount to deposit: ");
            if (scanf("%lf", &amt) != 1) { while(getchar()!='\n'); printf("Invalid.\n"); continue; }
            if (amt <= 0) { printf("Invalid amount.\n"); continue; }
            acc->balance += amt;
            add_txn(acc, "DEPOSIT", amt, 0);
            save_accounts();
            printf("Deposited %.2f. New balance: %.2f\n", amt, acc->balance);
        } else if (choice == 4) {
            long to_acc;
            double amt;
            printf("Enter recipient account number: ");
            if (scanf("%ld", &to_acc) != 1) { while(getchar()!='\n'); printf("Invalid.\n"); continue; }
            account_t *rcv = find_account(to_acc);
            if (!rcv) { printf("Recipient not found.\n"); continue; }
            printf("Enter amount to transfer: ");
            if (scanf("%lf", &amt) != 1) { while(getchar()!='\n'); printf("Invalid.\n"); continue; }
            if (amt <= 0) { printf("Invalid amount.\n"); continue; }
            if (amt > acc->balance) { printf("Insufficient funds.\n"); continue; }
            acc->balance -= amt;
            rcv->balance += amt;
            add_txn(acc, "TRANSFER_OUT", amt, to_acc);
            add_txn(rcv, "TRANSFER_IN", amt, acc->acc_no);
            save_accounts();
            printf("Transferred %.2f to %ld. Your new balance: %.2f\n", amt, to_acc, acc->balance);
        } else if (choice == 5) {
            show_mini_statement(acc);
        } else if (choice == 6) {
            int newpin;
            printf("Enter new 4-digit PIN: ");
            if (scanf("%d", &newpin) != 1) { while(getchar()!='\n'); printf("Invalid.\n"); continue; }
            acc->pin = newpin;
            save_accounts();
            printf("PIN changed successfully.\n");
        } else if (choice == 7) {
            printf("Logging out...\n");
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }
}

/* ---------- Main ---------- */
void print_menu() {
    printf("\n==== ATM SIMULATION ====\n");
    printf("1) Create new account\n2) Login to account\n3) Exit\nChoose: ");
}

int main() {
    load_accounts();
    int choice;
    while (1) {
        print_menu();
        if (scanf("%d", &choice) != 1) { while(getchar()!='\n'); printf("Invalid.\n"); continue; }
        if (choice == 1) {
            create_account();
        } else if (choice == 2) {
            account_t *acc = NULL;
            if (login_account(&acc)) {
                atm_session(acc);
            }
        } else if (choice == 3) {
            printf("Goodbye!\n");
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }
    return 0;
}