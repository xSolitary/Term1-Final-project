#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int orderidexist(int id){
    FILE *file = fopen("orders.csv", "r");
    if (!file) return 0;      
    char line[512];
    
    if (!fgets(line, sizeof line, file)) { fclose(file); return 0; }
    while (fgets(line, sizeof line, file)) {
        int cur;
        if (sscanf(line, "%d", &cur) == 1 && cur == id) { fclose(file); return 1; }
    }
    fclose(file);
    return 0;
}

void Addcsv(){
    FILE *file = fopen("orders.csv", "a");
    int orderid;
    char customername[30];
    char productname[30];
    int quantity;
    float price;
    char orderdate[12];
    printf("Enter orderID: ");
    scanf("%d",&orderid);

    if (orderidexist(orderid)) {
        printf("OrderID %d already exists. Not adding.\n", orderid);
        return;
    }

    printf("Enter customername: ");
    scanf("%s",customername);
    printf("Enter productname: ");
    scanf("%s", productname);
    printf("Enter quantity: ");
    scanf("%d", &quantity);
    printf("Enter price: ");
    scanf("%f", &price);
    printf("Enter orderdate: ");
    scanf("%s",orderdate);

    if (fprintf(file, "%d,%s,%s,%d,%.2f,%s\n",
                orderid, customername, productname, quantity, price, orderdate) < 0) {
        perror("fprintf");
    }
    fclose(file);

    
}
void menu(){
   
    while(1){
        printf("\n====================================\n");
        printf("[1]Add new Buy Order\n");
        printf("[2]Search Buy Order\n");
        printf("[3]Update Buy Order\n");
        printf("[4]Delete Buy Order\n");
        printf("[0]Exit\n");
        printf("====================================\n");
        printf("What action do you want to do.\n");
        int userchoose;
        scanf("%d",&userchoose);
        
        switch (userchoose)
        {
        case 1:
            Addcsv();
            // printf("1");
        
            break;
        case 2:
            printf("2");
            break;
        case 3:
            printf("3");
            break;
        case 4:
            printf("");
            break;
        case 0:
            return;
        default:
            printf("please enter 1-4 to do an action");
            
        }
    }
}
int main() {
    menu();
    return 0;
}