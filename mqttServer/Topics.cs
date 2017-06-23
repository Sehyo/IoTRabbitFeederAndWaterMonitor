using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MQTTClientIOT
{
    /*
    * Static Class just holds info about topic names
    */
    static public class Topics
    {
        public const String timeToFeedTopic = "hello/food",
        waterTopic = "unikent/users/adjn2/iot/waterLevel",
        foodRefillTopic = "hello/foodRefill";
    }
}
